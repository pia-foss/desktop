// Copyright (c) 2020 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include "common.h"
#line SOURCE_FILE("wireguardmethod.cpp")

#include "wireguardmethod.h"
#include "daemon.h"
#include "apiclient.h"
#include "wireguardbackend.h"
#include "exec.h"
#include "openssl.h"
#include "path.h"
#include <QTimer>
#include <QRandomGenerator>
#include <cstring>

#if defined(Q_OS_LINUX)
    #include "linux/wireguardkernelbackend.h"
    #include "linux/linux_fwmark.h"
    #include "linux/linux_routing.h"
#endif

#if defined(Q_OS_WIN)
    #include "win/wireguardservicebackend.h"
    #include "win/win_interfacemonitor.h"
    #include "win/win_util.h"
#endif

#if defined(Q_OS_UNIX)
    #include "posix/wireguardgobackend.h"
#endif

// embeddable-wg-library - C header
extern "C"
{
    #include <wireguard.h>
}

namespace
{
    const uint16_t keepaliveIntervalSec = 25;

    const std::chrono::seconds statsInterval{5};

    // Creating the interface must complete within this timeout
    const std::chrono::seconds createInterfaceTimeout{10};
    // Interval of checks for the first handshake - should be much faster than
    // the stat interval to detect the handshake promptly
    const std::chrono::milliseconds firstHandshakeInterval{200};
    // If the first handshake doesn't occur for this long after the interface is
    // up, the connection is failed.
    const std::chrono::seconds firstHandshakeTimeout{10};

    // Fetching stats must complete within this timeout
    const std::chrono::seconds statFetchTimeout{1};

    // If no handshake occurs for this amount of time, the connection is
    // abandoned.
    const std::chrono::minutes handshakeAbandonTimeout{4};

    // For brevity, only trace the handshake time just after it is renewed, or
    // if it isn't renewed for a long time
    const std::chrono::seconds handshakeTraceThreshold{15};
    const std::chrono::minutes handshakeWarnThreshold{3};

    // If no data is received for this amount of time, fire off a ping.
    // Should be a multiple of statsInterval.
    const std::chrono::seconds pingThreshold{25};

    // If no data is received for this amount of time after a ping is sent,
    // the connection is abandoned.
    // Should be a multiple of statsInterval.
    const std::chrono::seconds pingTimeout{pingThreshold + std::chrono::seconds{25}};

    // The ping threshold and timeout are measured in stat interval counts to
    // avoid off-by-one errors due to clock skew.
    unsigned pingThresholdIntervals = pingThreshold / statsInterval;
    unsigned pingTimeoutIntervals = pingTimeout / statsInterval;

    // How long the backend gets to shut down.  This is pretty long - the
    // backend should not take this long, but aborting during a shutdown may
    // cause worse problems.  In particular, shutdown on Windows can take a long
    // time if the service does not respond, and there's nothing we can do to
    // speed it up.
    //
    // After 1 minute though, if we haven't shut down, we time out to avoid
    // getting completely stuck.
    const std::chrono::minutes shutdownTimeout{1};

    QByteArray readPiaRsa4096CAPem()
    {
        QFile caFile{QStringLiteral(":/ca/rsa_4096.crt")};
        caFile.open(QIODevice::OpenModeFlag::ReadOnly | QIODevice::OpenModeFlag::Text);
        return caFile.readAll();
    }

    std::shared_ptr<PrivateCA> getPiaRsa4096CA()
    {
        static auto pCA{std::make_shared<PrivateCA>(readPiaRsa4096CAPem())};
        return pCA;
    }
}

class WireguardKeypair
{
public:
    WireguardKeypair()
    {
        static_assert(sizeof(_privateKey) == Curve25519KeySize, "WireGuard key is not expected size");
        if(!genCurve25519KeyPair(_publicKey, _privateKey))
        {
            qWarning() << "Could not generate WireGuard key pair";
            throw Error{HERE, Error::Code::WireguardCreateDeviceFailed};
        }
    }

public:
    const wg_key &privateKey() const {return _privateKey;}
    // No need to render the private key as a string currently
    const wg_key &publicKey() const {return _publicKey;}
    QString publicKeyStr() const {return wgKeyToB64(_publicKey);}

private:
    wg_key _privateKey, _publicKey;
};

// WireguardMethod is a VPNMethod that connects with Wireguard using any
// WireguardBackend.
class WireguardMethod : public VPNMethod
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("wireguardmethod")

private:
    struct AuthResult
    {
        // Fields in host byte order when relevant
        QPair<QHostAddress, int> _peerIpNet;
        wg_key _serverPubkey;
        QHostAddress _serverIp;
        quint16 _serverPort;
        QStringList _piaDnsServers;
        QHostAddress _serverVirtualIp;
    };

public:
    static void cleanup();

public:
    WireguardMethod(QObject *pParent, const OriginalNetworkScan &netScan);
    ~WireguardMethod() override;

private:
    // Delete the PIA Wireguard interface, if it exists
    void deleteInterface();

    void handleAuthResult(const WireguardKeypair &clientKeypair,
                          const QJsonDocument &result);
    AuthResult parseAuthResult(const QJsonDocument &result);
    void createInterface(const WireguardKeypair &clientKeypair,
                         const AuthResult &authResult);

    // Calculate a default MTU on Mac/Linux when no MTU was specified.  Uses
    // the link's MTU minus 80 bytes for encapsulation.
    QString findInterfaceForHost(const QString &host);
    QString findDefaultInterface();
    unsigned findInterfaceMtu(const QString &itf);
    unsigned findHostMtu(const QString &host);
    unsigned determinePosixMtu(const QHostAddress &host);

    void finalizeInterface(const QString &deviceName,
                           const AuthResult &authResult);

    // Bring up DNS on MacOs/Linux
    bool setupPosixDNS(const QString &deviceName, const QStringList &dnsServers);

    // Tear down DNS on MacOs/Linux
    static void teardownPosixDNS();

    // Get the Wireguard device for stats, handshake checks, etc.
    // Rejects the task if:
    // - the device can't be found
    // - the device has no peers
    Async<WireguardBackend::WgDevPtr> getWireguardDevice();

    // Check the last handshake time of the peer - stop the connect timer once
    // a handshake occurs, trace if needed, and abandon if it exceeds the
    // abandon threshold
    void checkPeerHandshake(const wg_device &dev);

    void checkFirstHandshake();

    // Update stats with the latest information from the adapter
    void updateStats();

    // Ping the endpoint
    void pingEndpoint();

    // Verify that pings are hitting endpoiint
    void checkPing(const quint64 &rx, const quint64 &tx);

    void checkDNS();
    void fixDNS(const QByteArray &existingContent, const QByteArray &expectedContent);

public:
    virtual void run(const ConnectionConfig &connectingConfig,
                     const Transport &transport,
                     const QHostAddress &localAddress,
                     quint16 shadowsocksProxyPort) override;
    virtual void shutdown() override;
    // Linux: the network adapter refers to the fixed device name.
    // Mac: the network adapter refers to the utun device obtained.
    // Windows: the network adapter contains the "adapter name" (a GUID) and the
    //          interface LUID.
    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() const override {return _pNetworkAdapter;}

private:
    // Authentication API request - set once the request is started (remains set
    // after that).
    Async<void> _pAuthRequest;
    // Backend implementation - set once we try to create the device, cleared
    // when we shut down
    std::unique_ptr<WireguardBackend> _pBackend;
    std::shared_ptr<NetworkAdapter> _pNetworkAdapter;
    // Elapsed time while checking for the first handshake
    QElapsedTimer _firstHandshakeElapsed;
    // First handshake timer - used to check frequently for the first handshake
    // until firstHandshakeTimeout elapses
    QTimer _firstHandshakeTimer;
    // Stats timer - started when WG interface is configured
    QTimer _statsTimer;
    // Connection configuration
    ConnectionConfig _connectionConfig;
    // The address of the VPN host
    QHostAddress _vpnHost;

    // Number of consecutive intervals with no new data received
    unsigned _noRxIntervals;

    // The last cumulative received byte count
    quint64 _lastReceivedBytes;

    // The address for the ping endpoint
    QHostAddress _pingEndpointAddress;

    QStringList _dnsServers;

    // Used to execute 'ip' commands with appropriate logging categories
    static Executor _executor;
};

Executor WireguardMethod::_executor{CURRENT_CATEGORY};

WireguardMethod::WireguardMethod(QObject *pParent, const OriginalNetworkScan &netScan)
    : VPNMethod{pParent, netScan}, _noRxIntervals{0}, _lastReceivedBytes{0}
{
    _firstHandshakeTimer.setInterval(msec(firstHandshakeInterval));
    connect(&_firstHandshakeTimer, &QTimer::timeout, this,
        &WireguardMethod::checkFirstHandshake);
    _statsTimer.setInterval(msec(statsInterval));
    connect(&_statsTimer, &QTimer::timeout, this,
        &WireguardMethod::updateStats);
}

WireguardMethod::~WireguardMethod()
{
    // Teardown is done when reaching the Exiting state.  If, somehow, the
    // method is destroyed without reaching this state, tear down the connection
    // to ensure it isn't left up.
    if(state() < State::Exiting)
    {
        qWarning() << "WireGuard method destroyed in state" << traceEnum(state());
        deleteInterface();
    }
}

bool WireguardMethod::setupPosixDNS(const QString &deviceName, const QStringList &dnsServers)
{
    // Note: we reuse the openvpn updown script here
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

#ifdef Q_OS_MACOS
    // Kill wireguard-go if the local network connection goes down
    QString killPid{QStringLiteral("0")};
    if(_pBackend)
        killPid = QString::number(_pBackend->killPid());
    env.insert("kill_pid", killPid);
#endif
    env.insert("dev", deviceName);
    env.insert("script_type", "up");
    QStringList argList;
    if(!dnsServers.isEmpty())
    {
        argList = QStringList{"--dns", dnsServers.join(':')};
    }

    // process status codes are 0 for success and non-zero for failure
    // so we need to flip it
    return !_executor.cmdWithEnv(Path::OpenVPNUpDownScript, argList, env);
}

void WireguardMethod::teardownPosixDNS()
{
    // We re-use the openvpn updown script to teardown DNS
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#if defined(Q_OS_LINUX)
    // The "down" implementation on Linux requires the interface name, which is
    // fixed on Linux.  On Mac, the interface name is not fixed, so we don't
    // know it during initial cleanup.
    env.insert("dev", WireguardBackend::interfaceName);
#endif
    env.insert("script_type", "down");
    _executor.cmdWithEnv(Path::OpenVPNUpDownScript, {}, env);
}

void WireguardMethod::deleteInterface()
{
    // Routing/DNS cleanup
#if defined(Q_OS_LINUX)
    teardownPosixDNS();

    // Remove routing rule for Wireguard (this is safe even if no rule exists)
    _executor.bash(QStringLiteral("ip rule del not from all fwmark %1 lookup %2")
                   .arg(Fwmark::wireguardFwmark).arg(Routing::wireguardTable));

#elif defined(Q_OS_MAC)
    teardownPosixDNS();

    // Delete the VPN route
    _executor.bash(QStringLiteral("route delete %1").arg(_vpnHost.toString()));
#elif defined(Q_OS_WIN)
    // Nothing to do on Windows.  The backend takes care of the routes, and
    // destroying the interface takes care of DNS.
#endif
    if(_pNetworkAdapter)
        _pNetworkAdapter->restoreOriginalMetric();
    _pNetworkAdapter.reset();
    _vpnHost.clear();
    // Destroy the backend to tear down the interface, if it was created
    _pBackend.reset();
}

void WireguardMethod::handleAuthResult(const WireguardKeypair &clientKeypair,
                                       const QJsonDocument &result)
{
    auto authResult = parseAuthResult(result);

    auto serverPubkeyTrace = wgKeyToB64(authResult._serverPubkey);
    qInfo().nospace() << "Server address: " << authResult._serverIp << ":"
        << authResult._serverPort << " peer IP: "
        << authResult._peerIpNet.first.toString() << "/"
        << authResult._peerIpNet.second << " server pubkey: "
        << serverPubkeyTrace;

    createInterface(clientKeypair, authResult);
}

auto WireguardMethod::parseAuthResult(const QJsonDocument &result)
    -> AuthResult
{
    const auto &resultStatus = result[QStringLiteral("status")].toString();
    if(resultStatus != QStringLiteral("OK"))
    {
        qWarning() << "Server rejected key, status:" << resultStatus;
        qWarning().noquote() << "Server response:" << result.toJson();
        throw Error{HERE, Error::Code::WireguardAddKeyFailed};
    }

    const auto &piaDnsServersArray = result[QStringLiteral("dns_servers")].toArray();
    const auto &peerIpStr = result[QStringLiteral("peer_ip")].toString();
    const auto &serverPubkeyStr = result[QStringLiteral("server_key")].toString();
    const auto &serverIpStr = result[QStringLiteral("server_ip")].toString();
    const auto &serverPortVal = result[QStringLiteral("server_port")];
    const auto &serverVipStr = result[QStringLiteral("server_vip")].toString();
    // Don't care about peer_pubkey, we already know our own public key

    // Sanity check
    int serverPort = serverPortVal.toInt();
    if(serverPort <= 0 || serverPort > std::numeric_limits<quint16>::max())
    {
        qWarning() << "Invalid server port:" << serverPortVal << "->"
            << serverPort;
        throw Error{HERE, Error::Code::WireguardAddKeyFailed};
    }

    QHostAddress serverIp{serverIpStr};
    if(serverIp.protocol() != QAbstractSocket::NetworkLayerProtocol::IPv4Protocol)
    {
        qWarning() << "Invalid server IP:" << serverIpStr << "->" << serverIp;
        throw Error{HERE, Error::Code::WireguardAddKeyFailed};
    }

    QHostAddress serverVip{serverVipStr};
    if(serverVip.protocol() != QAbstractSocket::NetworkLayerProtocol::IPv4Protocol)
    {
        qWarning() << "Invalid server virtual IP:" << serverVipStr << "->"
            << serverVip;
        throw Error{HERE, Error::Code::WireguardAddKeyFailed};
    }

    QStringList piaDnsServers;
    piaDnsServers.reserve(piaDnsServersArray.size());
    for(const auto &address : piaDnsServersArray)
    {
        const auto &addressStr = address.toString();
        if(!addressStr.isEmpty())
            piaDnsServers.push_back(addressStr);
    }
    if(piaDnsServers.isEmpty())
    {
        qWarning().noquote() << "Invalid DNS server addresses:"
            << QJsonDocument{piaDnsServersArray}.toJson();
        throw Error{HERE, Error::Code::WireguardAddKeyFailed};
    }

    AuthResult authResult;
    authResult._serverIp = std::move(serverIp);
    authResult._serverPort = static_cast<quint16>(serverPort);
    authResult._serverVirtualIp = std::move(serverVip);
    authResult._piaDnsServers = std::move(piaDnsServers);

    authResult._peerIpNet = QHostAddress::parseSubnet(peerIpStr);
    if(authResult._peerIpNet.first.isNull() ||
        authResult._peerIpNet.first.protocol() != QAbstractSocket::NetworkLayerProtocol::IPv4Protocol ||
        authResult._peerIpNet.second <= 0 || authResult._peerIpNet.second > 32)
    {
        qWarning() << "Invalid peer IP:" << peerIpStr << "->"
            << authResult._peerIpNet.first << "/"
            << authResult._peerIpNet.second;
        throw Error{HERE, Error::Code::WireguardAddKeyFailed};
    }

    QByteArray serverPubkey = QByteArray::fromBase64(serverPubkeyStr.toLatin1());
    if(serverPubkey.size() != sizeof(authResult._serverPubkey))
    {
        qWarning() << "Invalid server public key (len" << serverPubkey.size()
            << "):" << serverPubkeyStr;
        throw Error{HERE, Error::Code::WireguardAddKeyFailed};
    }

    std::copy(serverPubkey.begin(), serverPubkey.end(), std::begin(authResult._serverPubkey));

    return authResult;
}

void WireguardMethod::createInterface(const WireguardKeypair &clientKeypair,
                                      const AuthResult &authResult)
{
    // Set up a device with 1 peer, which in turn has 1 allowed IP range
    wg_device wgDev{};
    wg_peer serverPeer{};
    wg_allowedip anyIpv4Allowed{};
    wgDev.first_peer = wgDev.last_peer = &serverPeer;
    serverPeer.first_allowedip = serverPeer.last_allowedip = &anyIpv4Allowed;

    // Specify the device
    wgDev.flags = static_cast<wg_device_flags>(WGDEVICE_REPLACE_PEERS |
                                               WGDEVICE_HAS_PRIVATE_KEY);
#if defined(Q_OS_LINUX)
    // Set the fwmark, this is Linux-specific.
    wgDev.flags = static_cast<wg_device_flags>(wgDev.flags | WGDEVICE_HAS_FWMARK);
    wgDev.fwmark = Fwmark::wireguardFwmark;
#endif

    if(_connectionConfig.localPort())
    {
        wgDev.listen_port = _connectionConfig.localPort();
        wgDev.flags = static_cast<wg_device_flags>(wgDev.flags | WGDEVICE_HAS_LISTEN_PORT);
    }

    // Just specify the private key.  UAPI doesn't support the public key at
    // all, and kernel mode presumably does not need it on a set (since it can
    // be derived from the private key anyway).
    std::copy(std::begin(clientKeypair.privateKey()),
              std::end(clientKeypair.privateKey()),
              std::begin(wgDev.private_key));

    // Specify the server peer
    serverPeer.flags = static_cast<wg_peer_flags>(WGPEER_REPLACE_ALLOWEDIPS |
                                                  WGPEER_HAS_PUBLIC_KEY |
                                                  WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL);
    std::copy(std::begin(authResult._serverPubkey),
              std::end(authResult._serverPubkey),
              std::begin(serverPeer.public_key));
    serverPeer.endpoint.addr4.sin_family = AF_INET;
    serverPeer.endpoint.addr4.sin_port = htons(authResult._serverPort);
    serverPeer.endpoint.addr4.sin_addr.s_addr = htonl(authResult._serverIp.toIPv4Address());
    serverPeer.persistent_keepalive_interval = keepaliveIntervalSec;

    // Specify the allowed IPs for that peer (0.0.0.0/0)
    anyIpv4Allowed.family = AF_INET;
    anyIpv4Allowed.ip4.s_addr = 0;
    anyIpv4Allowed.cidr = 0;

    // Create the backend in order to create the interface
#if defined(Q_OS_LINUX)
    // Prefer the kernel backend if it's enabled and available
    if(_connectionConfig.wireguardUseKernel() && g_state.wireguardKernelSupport())
        _pBackend.reset(new WireguardKernelBackend{});
#endif
#if defined(Q_OS_WIN)
    _pBackend.reset(new WireguardServiceBackend{});
#endif
#if defined(Q_OS_UNIX)
    // On Linux, the kernel backend is preferred, but if that's not suitable,
    // use the userspace backend.
    if(!_pBackend)
        _pBackend.reset(new WireguardGoBackend{});
#endif

    if(!_pBackend)
    {
        qWarning() << "Could not create WireGuard backend";
        throw Error{HERE, Error::WireguardCreateDeviceFailed};
    }

    connect(_pBackend.get(), &WireguardBackend::error, this,
            &WireguardMethod::raiseError);

    // Persist the VPN host IP
    _vpnHost = authResult._serverIp;

    // Ping the server's virtual IP to test connectivity
    _pingEndpointAddress = authResult._serverVirtualIp;

    // Reset to 0 before connect
    _noRxIntervals = 0;
    _lastReceivedBytes = 0;

    // Create the device; this throws if the device can't be created.
    // The backend may modify wgDev, we don't use it after this point.
    _pBackend->createInterface(wgDev, authResult._peerIpNet)
        .timeout(createInterfaceTimeout)
        ->notify(this, [this, authResult](const Error &err, const std::shared_ptr<NetworkAdapter> &pDevice)
        {
            if(err || !pDevice)
            {
                qWarning() << "Could not create interface:" << err;
                raiseError(err);
                return;
            }

            if(state() >= State::Exiting)
            {
                qWarning() << "Ignoring connect result, already advanced to state"
                    << traceEnum(state());
                return;
            }

            // Otherwise, if we are not exiting, we must be in the connecting
            // state - the Connected transition occurs after starting the
            // handshake timer
            Q_ASSERT(state() == State::Connecting);

            // The interface is up, store the NetworkAdapter
            _pNetworkAdapter = pDevice;
            _pNetworkAdapter->setMetricToLowest();
            // The network adapter is used by the firewall, ensure that it updates
            emitFirewallParamsChanged();

            // Bring up the interface and configure routing and DNS
            finalizeInterface(pDevice->devNode(), authResult);

            // We're not "connected" yet - wait for a handshake to complete
            _firstHandshakeElapsed.start();
            _firstHandshakeTimer.start();
            _statsTimer.start();
        });
}

QString WireguardMethod::findInterfaceForHost(const QString &host)
{
#if defined(Q_OS_MACOS)
    auto hostItfMatch = _executor.cmdWithRegex(QStringLiteral("route"),
        {QStringLiteral("-n"), QStringLiteral("get"), QStringLiteral("-inet"), host},
        QRegularExpression{QStringLiteral("interface: ([^ ]+)$"), QRegularExpression::PatternOption::MultilineOption});
    if(hostItfMatch.hasMatch())
        return hostItfMatch.captured(1);
#elif defined(Q_OS_LINUX)
    auto hostItfMatch = _executor.cmdWithRegex(QStringLiteral("ip"),
        {QStringLiteral("route"), QStringLiteral("get"), host},
        QRegularExpression{QStringLiteral("dev ([^ ]+)")});
    if(hostItfMatch.hasMatch())
        return hostItfMatch.captured(1);
#endif
    return {};
}

QString WireguardMethod::findDefaultInterface()
{
#if defined(Q_OS_MACOS)
    auto defItfMatch = _executor.cmdWithRegex(QStringLiteral("route"),
        {QStringLiteral("-n"), QStringLiteral("get"), QStringLiteral("-inet"), QStringLiteral("default")},
        QRegularExpression{QStringLiteral("interface: ([^ ]+)$"), QRegularExpression::PatternOption::MultilineOption});
    if(defItfMatch.hasMatch())
        return defItfMatch.captured(1);
#elif defined(Q_OS_LINUX)
    auto hostItfMatch = _executor.cmdWithRegex(QStringLiteral("ip"),
        {QStringLiteral("route"), QStringLiteral("show"), QStringLiteral("default")},
        QRegularExpression{QStringLiteral("dev ([^ ]+)")});
    if(hostItfMatch.hasMatch())
        return hostItfMatch.captured(1);
#endif
    return {};
}

unsigned WireguardMethod::findInterfaceMtu(const QString &itf)
{
    QString mtuStr;
#if defined(Q_OS_MACOS)
    auto itfMtuMatch = _executor.cmdWithRegex(QStringLiteral("ifconfig"),
        {itf}, QRegularExpression{QStringLiteral("mtu ([0-9]+)")});
    if(itfMtuMatch.hasMatch())
        mtuStr = itfMtuMatch.captured(1);
#elif defined(Q_OS_LINUX)
    auto itfMtuMatch = _executor.cmdWithRegex(QStringLiteral("ip"),
        {QStringLiteral("link"), QStringLiteral("show"), QStringLiteral("dev"),
         itf}, QRegularExpression{QStringLiteral("mtu ([0-9]+)")});
    if(itfMtuMatch.hasMatch())
        mtuStr = itfMtuMatch.captured(1);
#endif
    return mtuStr.toUInt();
}

unsigned WireguardMethod::findHostMtu(const QString &host)
{
    const auto &hostItf = findInterfaceForHost(host);
    if(!hostItf.isEmpty())
    {
        // Found an interface for the specified host.  Return its MTU, or 0 if
        // the interface doesn't have an MTU specified.  (It doesn't seem
        // sensible to check the default route since we know what interface will
        // be used for this host; if they were different we'd just be applying
        // the MTU from some other irrelevant interface.)
        unsigned mtu = findInterfaceMtu(hostItf);
        qInfo() << "Found MTU" << mtu << "from interface" << hostItf
            << "to host" << host;
        return mtu;
    }

    // We couldn't find an interface for that host, try to find the default
    // interface.
    const auto &defaultItf = findDefaultInterface();
    if(!defaultItf.isEmpty())
    {
        unsigned mtu = findInterfaceMtu(defaultItf);
        qInfo() << "Found MTU" << mtu << "from default interface" << defaultItf;
        return mtu;
    }

    return 0;
}

unsigned WireguardMethod::determinePosixMtu(const QHostAddress &host)
{
    unsigned mtu = _connectionConfig.mtu();
    if(mtu)
        return mtu;

    // Find an MTU if none was specified.
    // Find the MTU for the interface used to reach the remote host.
    mtu = findHostMtu(host.toString());
    // Default to 1500 if no MTU was found.
    if(!mtu)
        mtu = 1500;
    // Subtract 80 bytes for encapsulation.
    mtu -= 80;

    return mtu;
}

void WireguardMethod::finalizeInterface(const QString &deviceName, const AuthResult &authResult)
{
    TraceStopwatch stopwatch{"Configuring WireGuard interface"};

    const auto &peerIpNet = authResult._peerIpNet;
    _dnsServers = _connectionConfig.getDnsServers(authResult._piaDnsServers);

// OS specific interface config (including DNS and routing)
#if defined(Q_OS_LINUX)
    // Setup the interface
    if(_executor.bash(QStringLiteral("ip addr add %1/%2 dev %3")
        .arg(peerIpNet.first.toString())
        .arg(peerIpNet.second)
        .arg(deviceName)))
    {
        throw Error{HERE, Error::Code::WireguardConfigDeviceFailed};
    }

    // MTU (also brings up interface)
    unsigned mtu = determinePosixMtu(authResult._serverIp);
    if(_executor.bash(QStringLiteral("ip link set mtu %1 up dev %2").arg(mtu).arg(deviceName)))
        throw Error{HERE, Error::Code::WireguardConfigDeviceFailed};

    // Routing
    // Ensure rule priority is less than split tunnel rule priorities (which are set at 100)
    // This rule sends all normal packets through the Wireguard interface, effectively setting the Wireguard interface as the default route
    // All wireguard packets (i.e those going to the wg endpoint) will fall back to the pre-existing gateway and so out the physical interface
    _executor.bash(QStringLiteral("ip rule add not fwmark %1 lookup %2 pri 101")
        .arg(Fwmark::wireguardFwmark).arg(Routing::wireguardTable));
    _executor.bash(QStringLiteral("ip route replace default dev %1 table %2").arg(WireguardBackend::interfaceName, Routing::wireguardTable));

    // DNS
    if(!setupPosixDNS(deviceName, _dnsServers))
    {
        // Only Linux has support for DNS config errors
        raiseError(Error(HERE, Error::OpenVPNDNSConfigError));
    }
#elif defined(Q_OS_MACOS)
    // Setup the interface
    _executor.bash(QStringLiteral("ifconfig %1 inet %2/%3 %2 alias").arg(deviceName, peerIpNet.first.toString(), QString::number(peerIpNet.second)));
    _executor.bash(QStringLiteral("ifconfig %1 up").arg(deviceName));

    // MTU
    unsigned mtu = determinePosixMtu(authResult._serverIp);
    _executor.bash(QStringLiteral("ifconfig %1 mtu %2").arg(deviceName).arg(mtu));

    // Routing
    _executor.bash(QStringLiteral("route -q -n add -inet 0.0.0.0/1 -interface %1").arg(deviceName));
    _executor.bash(QStringLiteral("route -q -n add -inet 128.0.0.0/1 -interface %1").arg(deviceName));
    _executor.bash(QStringLiteral("route -q -n add -inet %1 -gateway %2").arg(_vpnHost.toString(), _netScan.gatewayIp()));

    // DNS
    setupPosixDNS(deviceName, _dnsServers);
#elif defined(Q_OS_WIN)
    auto pWinAdapter = std::static_pointer_cast<WinNetworkAdapter>(_pNetworkAdapter);

    // MTU
    //
    // The WireGuard service supports setting the MTU, but it doesn't work; it
    // always fails with "the parameter is incorrect".  Do it manually instead.
    //
    // If the MTU can't be set, we do not raise an error - proceed with the
    // connection without setting the MTU.
    //
    // Retrying won't fix this error; if we throw an error we'd just constantly
    // retry due to this failure.  We might add an in-client notification if it
    // fails a lot.
    unsigned mtu = _connectionConfig.mtu();
    if(mtu)
    {
        MIB_IPINTERFACE_ROW tunItf{};
        InitializeIpInterfaceEntry(&tunItf);
        tunItf.Family = AF_INET;
        tunItf.InterfaceLuid.Value = pWinAdapter->luid();
        auto getResult = GetIpInterfaceEntry(&tunItf);
        if(getResult != NO_ERROR)
        {
            qWarning() << "Unable to get interface state to set MTU to" << mtu
                << "-" << WinErrTracer{getResult};
        }
        else
        {
            tunItf.NlMtu = mtu;
            auto setResult = SetIpInterfaceEntry(&tunItf);
            if(setResult != NO_ERROR)
            {
                qWarning() << "Unable to set interface MTU to" << mtu << "-"
                    << WinErrTracer{setResult};
            }
            else
            {
                qInfo() << "Set interface MTU to" << mtu;
            }
        }
    }

    if(!_dnsServers.isEmpty())
    {
        // Reset DNS on this interface
        QString nameParam{QStringLiteral("name=%1").arg(pWinAdapter->indexIpv4())};
        _executor.cmd(QStringLiteral("netsh"), {"interface", "ipv4", "set",
            "dnsservers", nameParam, "source=static", "address=none", "validate=no",
            "register=both"});
        // Add each DNS server
        for(const auto &dns : _dnsServers)
        {
            QString addressParam{QStringLiteral("address=%1").arg(dns)};
            _executor.cmd(QStringLiteral("netsh"), {"interface", "ipv4", "add",
                "dnsservers", nameParam, addressParam, "validate=no"});
        }
    }
#endif

    emitTunnelConfiguration(deviceName, peerIpNet.first.toString(), {},
                            _dnsServers);
}

auto WireguardMethod::getWireguardDevice() -> Async<WireguardBackend::WgDevPtr>
{
    if(!_pBackend)
    {
        qWarning() << "Can't get WireGuard device status - backend not created or was destroyed";
        raiseError({HERE, Error::Code::WireguardDeviceLost});
        return {};
    }

    return _pBackend->getStatus()
        ->next([](const Error &err, const WireguardBackend::WgDevPtr &pDev)
            {
                if(err || !pDev)
                {
                    qWarning() << "Can't find WireGuard device for stats";
                    throw err;
                }
                // If, somehow, we have no peers, consider the connection lost
                if(!pDev->first_peer)
                {
                    qWarning() << "No peers on WireGuard interface";
                    throw Error{HERE, Error::Code::WireguardDeviceLost};
                }
                return pDev;
            });
}

void WireguardMethod::checkPeerHandshake(const wg_device &dev)
{
    Q_ASSERT(dev.first_peer);   // Ensured by caller, postcondition of getWireguardDevice()

    // Check the peer handshake time, to make sure the connection is established
    // and to abandon a lost connection.
    //
    // There normally is only one peer, but for robustness, consider the newest
    // handshake if there is more than one peer.
    int64_t lastHandshakeTime{0};
    for(auto pPeer = dev.first_peer; pPeer; pPeer = pPeer->next_peer)
    {
        // Only care about seconds
        if(pPeer->last_handshake_time.tv_sec > lastHandshakeTime)
            lastHandshakeTime = pPeer->last_handshake_time.tv_sec;
    }

    if(lastHandshakeTime == 0)
    {
        qInfo() << "No handshake yet";
        return;
    }

    time_t now = time(nullptr);
    // Since we got a handshake, advance to Connected and stop the
    // failure timer (if we haven't yet)
    advanceState(State::Connected);
    _firstHandshakeTimer.stop();

    std::chrono::seconds handshakeTimeAgo{now - lastHandshakeTime};
    if(handshakeTimeAgo < handshakeTraceThreshold)
    {
        qInfo() << "peer: handshake at"
            << lastHandshakeTime << "-" << traceMsec(handshakeTimeAgo) << "ago";
    }
    if(handshakeTimeAgo > handshakeWarnThreshold)
    {
        qWarning() << "peer: last handshake at"
            << lastHandshakeTime << "-" << traceMsec(handshakeTimeAgo) << "ago";
    }

    // If there hasn't been a handshake for an unexpected amount of time,
    // assume the connection is lost (the server may be gone, may have
    // forgotten our public key, etc.)
    if(handshakeTimeAgo > handshakeAbandonTimeout)
    {
        qWarning() << "Abandoning connection, last handshake at"
            << traceMsec(handshakeTimeAgo) << "ago exceeds limit of"
            << traceMsec(handshakeAbandonTimeout);
        raiseError({HERE, Error::Code::WireguardHandshakeTimeout});
    }
}

void WireguardMethod::checkFirstHandshake()
{
    if(state() != State::Connecting)
        return; // Nothing to do, already connected or exiting

    getWireguardDevice()
        .timeout(firstHandshakeInterval)
        ->notify(this, [this](const Error &err, const WireguardBackend::WgDevPtr &pDev)
            {
                // If we're not still in the Connecting state, there's nothing
                // to do, we already moved to another state
                if(state() != State::Connecting)
                {
                    qWarning() << "Ignoring first handshake stats, already advanced to state"
                        << traceEnum(state());
                    return;
                }
                if(err || !pDev)
                    return; // Traced by getWireguardDevice()

                checkPeerHandshake(*pDev);

                // If we haven't gone to the connected state, and the connection
                // has timed out, raise an error.
                auto elapsed = _firstHandshakeElapsed.elapsed();
                if(state() == State::Connecting)
                {
                    if(elapsed > msec(firstHandshakeTimeout))
                    {
                        qWarning() << "Connection timed out; no handshake after"
                            << traceMsec(elapsed);
                        raiseError({HERE, Error::Code::WireguardHandshakeTimeout});
                    }
                    else
                    {
                        qInfo() << "Still waiting for first handshake after"
                            << traceMsec(elapsed);
                    }
                }
            });
}

void WireguardMethod::updateStats()
{
    // If we're already exiting, there's nothing to do.
    if(state() >= State::Exiting)
        return;

    getWireguardDevice()
        .timeout(statFetchTimeout)
        ->notify(this, [this](const Error &err, const WireguardBackend::WgDevPtr &pDev)
            {
                // If we started exiting by the time the result arrived, there's
                // nothing to do
                if(state() >= State::Exiting)
                {
                    qWarning() << "Ignoring stat result, already advanced to state"
                        << traceEnum(state());
                    return;
                }
                if(err)
                {
                    qWarning() << "Failed to fetch device stats:" << err;
                    raiseError(err);
                    return;
                }

                // Add up bytecounts
                quint64 rx{0}, tx{0};
                for(auto pPeer = pDev->first_peer; pPeer; pPeer = pPeer->next_peer)
                {
                    rx += pPeer->rx_bytes;
                    tx += pPeer->tx_bytes;
                }

                // Trace bytecounts - this is pretty useful for diagnostics.
                // The OpenVPN method gets this trace from the management
                // interface, this is similar.
                qInfo().nospace() << "BYTECOUNT: " << rx << ", " << tx;
                emitBytecounts(rx, tx);

                checkPeerHandshake(*pDev);

                checkDNS();

                // Check the connection is still up
                checkPing(rx, tx);
            });
}

void WireguardMethod::fixDNS(const QByteArray &existingContent, const QByteArray &expectedContent)
{
    qWarning() << "DNS servers are not as expected."
               << QStringLiteral("Found %1 but expected %2")
                  .arg(QString{existingContent}).arg(QString{expectedContent});

    // Open resolv.conf for writing
    QFile resolvConf{"/etc/resolv.conf"};
    resolvConf.open(QFile::WriteOnly | QIODevice::Text);

    // Write the nameservers we expect
    resolvConf.write(expectedContent);
    qInfo() << "Updated /etc/resolv.conf with expected content";

    // Backup the contents of /etc/resolv.conf
    QFile resolvConfBackup{Path::DaemonDataDir/QStringLiteral("pia.resolv.conf")};
    resolvConfBackup.open(QFile::WriteOnly | QIODevice::Text);
    resolvConfBackup.write(existingContent);
}

void WireguardMethod::checkDNS()
{
#ifdef Q_OS_LINUX
    // Only need to check (+ fix) DNS for Linux systems where /etc/resolv.conf is not a symlink to
    // systemd-resolved or resolvconf stubs. This is because systemd-resolved/resolvconf allow per-interface
    // DNS settings which are protected against changes (as the wireguard interface is "unmanaged", see linux_installer.sh).
    if(!_dnsServers.isEmpty() && QFile::symLinkTarget("/etc/resolv.conf").isEmpty())
    {
        QStringList entries;
        for(const auto &server : _dnsServers)
            entries << QStringLiteral("nameserver %1").arg(server);

        QFile resolvConf{"/etc/resolv.conf"};

        // Open for reading
        resolvConf.open(QFile::ReadOnly | QIODevice::Text);

        QByteArray existingContent{resolvConf.readAll()};
        QByteArray expectedContent{entries.join('\n').append('\n').toLatin1()};
        resolvConf.close();

        if(existingContent != expectedContent)
            fixDNS(existingContent, expectedContent);
    }
#endif
}

void WireguardMethod::checkPing(const quint64 &rx, const quint64 &tx)
{
    // The change in bytes received
    quint64 receivedDelta = rx - _lastReceivedBytes;
    _lastReceivedBytes = rx;

    // If we're receiving data, then we still have a connection
    // So reset _noRxIntervals
    if(receivedDelta != 0)
    {
        _noRxIntervals = 0;
        return;
    }

    // Otherwise, this is an additional interval with no data
    ++_noRxIntervals;

    // If pingTimeout seconds have elapsed and we haven't yet received data, abort the connection
    if(_noRxIntervals >= pingTimeoutIntervals)
    {
        qWarning() << "Abandoning connection due to ping timeout."
            << "No response after" << traceMsec(statsInterval * _noRxIntervals);

         raiseError({HERE, Error::Code::WireguardPingTimeout});
         return;
    }

    // Otherwise if only pingThreshold seconds have elapsed since we last received data - start pinging the endpoint
    // This ping will be repeated each time checkPing() is called, which should be every 5 seconds
    if(_noRxIntervals >= pingThresholdIntervals)
    {
        qWarning() << "No data received in" << traceMsec(statsInterval * _noRxIntervals)
            << "- pinging endpoint";
        pingEndpoint();
    }
}

void WireguardMethod::pingEndpoint()
{
#if defined(Q_OS_UNIX)
    // -f flag should fire and forget - i.e not wait around for ICMP echo reply
    _executor.cmd("ping", {"-f", "-n", "-c", "1", "-W", "1", _pingEndpointAddress.toString()});
#elif defined(Q_OS_WIN)
    QProcess pingProc;
    pingProc.setProgram("ping");
    pingProc.setArguments({"/w", "1", "/n", "1", _pingEndpointAddress.toString()});

    // We do not need to reap the exit code on Windows (on Unix we do, otherwise we end up with a Zombie process)
    pingProc.startDetached();
#endif
}

void WireguardMethod::run(const ConnectionConfig &connectingConfig,
                          const Transport &transport,
                          const QHostAddress &localAddress,
                          quint16 shadowsocksProxyPort)
{
    advanceState(State::Connecting);

    // Generate a keypair, and push the public key to the server with our
    // credentials
    WireguardKeypair clientKeypair;

    // Store a copy of the connection config, we need things like DNS servers
    // later after the interface is created
    _connectionConfig = connectingConfig;

    if(!connectingConfig.vpnLocation())
    {
        qWarning() << "No VPN location specified";
        throw Error{HERE, Error::Code::VPNConfigInvalid};
    }
    QHostAddress wgHost{connectingConfig.vpnLocation()->wireguardUdpHost()};
    quint16 wgPort{connectingConfig.vpnLocation()->wireguardUdpPort()};
    if(wgHost.isNull() || !wgPort)
    {
        qWarning() << "UDP host" << connectingConfig.vpnLocation()->wireguardUDP()
            << "not valid in location" << connectingConfig.vpnLocation()->id();
        throw Error{HERE, Error::Code::VPNConfigInvalid};
    }
    const auto &certCommonName = connectingConfig.vpnLocation()->wireguardSerial();
    if(certCommonName.isEmpty())
    {
        qWarning() << "Certificate serial number not known for region"
            << connectingConfig.vpnLocation()->id();
        throw Error{HERE, Error::Code::VPNConfigInvalid};
    }

    QString authHost = QStringLiteral("https://") + wgHost.toString() + ":" +
        QString::number(wgPort);

    qInfo() << "Authenticating with server" << authHost
        << "with expected common name" << certCommonName;

    QString resource = QStringLiteral("addKey?pt=");
    resource += connectingConfig.vpnToken();
    resource += QStringLiteral("&pubkey=");
    resource += QString::fromLatin1(QUrl::toPercentEncoding(clientKeypair.publicKeyStr()));
    // Don't do DNS resolution while connecting - specify the IP address in the
    // request, and use the host name to verify the certificate.
    ApiBase hostAuthBase{authHost, getPiaRsa4096CA(), certCommonName};

    _pAuthRequest = ApiClient::instance()->getRetry(hostAuthBase, resource, {})
        ->then(this, [this, clientKeypair=std::move(clientKeypair)](const QJsonDocument &result)
            {
                handleAuthResult(clientKeypair, result);
            })
        ->except(this, [this](const Error &ex){raiseError(ex);});
}

void WireguardMethod::shutdown()
{
    // We could have already reached Exited; nothing to do in that case
    if(state() >= State::Exiting)
        return;

    _statsTimer.stop();
    _firstHandshakeTimer.stop();

    advanceState(State::Exiting);

    // Allow the backend to shut down
    Async<void> pShutdownTask;
    if(_pBackend)
    {
        qInfo() << "Shut down WireGuard backend";
        pShutdownTask = _pBackend->shutdown();
    }
    else
        pShutdownTask = Async<void>::resolve(); // Nothing to shut down

    // When that completes, or times out, tear down routes/DNS and internal
    // state
    pShutdownTask
        .timeout(shutdownTimeout)
        ->notify(this, [this](const Error &err)
        {
            if(err)
            {
                qWarning() << "Backend shutdown rejected -" << err;
                // Still continue to shut down
            }

            qInfo() << "Tearing down WireGuard connection";
            deleteInterface();

            // Go to Exited asynchronously, even if the backend resolved the
            // shutdown synchronously.
            // The UI killswitch warning currently expects to observe firewall
            // state updates before we reach Exited
            QTimer::singleShot(500, this, [this]()
            {
                qWarning() << "WireGuard shutdown complete";
                advanceState(State::Exited);
            });
        });
}

void WireguardMethod::cleanup()
{
    // Clean up any possible remnants of a WireGuard connection, in case the
    // daemon was to crash and restart.
    //
    // This is strictly a resiliency measure, if the daemon hasn't crashed it
    // shouldn't have any effect.  OpenVPN does not generally have this issue;
    // a daemon crash would cause it to shut down due to losing the management
    // interface.
    //
    // The daemon rarely crashes, but this cleanup limits the impact of such a
    // crash if it was to occur.  On most systems, the daemon will be restarted
    // automatically, so we should be able to get the system back to a
    // reasonable state.
#ifdef Q_OS_UNIX
    teardownPosixDNS();

    // On Mac, if the daemon was to crash and restart, it's possible that a
    // route to the VPN host might be left around - we don't know what host that
    // was, so we can't clean it up after a crash and restart.  This should
    // generally be OK, the route should usually be OK if the local interface
    // hasn't changed, and the route should be removed by the system if the
    // local interface does change.

    WireguardGoBackend::cleanup();
#endif
#ifdef Q_OS_LINUX
    // Remove routing rule for Wireguard (this is safe even if no rule exists)
    _executor.bash(QStringLiteral("ip rule del not from all fwmark %1 lookup %2")
                   .arg(Fwmark::wireguardFwmark).arg(Routing::wireguardTable));

    WireguardKernelBackend::cleanup();
#endif
#ifdef Q_OS_WIN
    WireguardServiceBackend::cleanup();
#endif
}

void cleanupWireguard()
{
    WireguardMethod::cleanup();
}

std::unique_ptr<VPNMethod> createWireguardMethod(QObject *pParent, const OriginalNetworkScan &netScan)
{
    return std::unique_ptr<VPNMethod>{new WireguardMethod{pParent, netScan}};
}

#include "wireguardmethod.moc"
