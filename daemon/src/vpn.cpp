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
#line SOURCE_FILE("connection.cpp")

#include "vpn.h"
#include "daemon.h"
#include "path.h"
#include "brand.h"

#include <QFile>
#include <QTextStream>
#include <QTcpSocket>
#include <QTimer>
#include <QHostInfo>
#include <QRandomGenerator>

// For use by findInterfaceIp on Mac/Linux
#ifdef Q_OS_UNIX
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#endif

// List of settings which require a reconnection.
static std::initializer_list<const char*> g_connectionSettingNames = {
    "location",
    "protocol",
    "remotePortUDP",
    "remotePortTCP",
    "localPort",
    "automaticTransport",
    "cipher",
    "auth",
    "serverCertificate",
    "overrideDNS",
    "defaultRoute",
    "blockIPv6",
    "mtu",
    "enableMACE",
    "windowsIpMethod",
    "proxy",
    "proxyCustom",
    "proxyShadowsocksLocation"
};

namespace
{
    // Maximum number of measurements in _intervalMeasurements
    const std::size_t g_maxMeasurementIntervals{32};

    // This seed is run by PIA Ops, this is used in addition to hnsd's
    // hard-coded seeds.  It has a static IP address but it's also resolvable
    // as hsd.londontrustmedia.com.
    //
    // The base32 blob is a public key used to authenticate the node.
    const QString hnsdSeed = QStringLiteral("aixqwzdsxogyjqwxjtdm27pqv7d23wkxi4tynqcfnqgpefsmbemj2@209.95.51.184");
    // This seed is run by HNScan.
    const QString hnscanSeed = QStringLiteral("ak2hy7feae2o5pfzsdzw3cxkxsu3lxypykcl6iphnup4adf2ply6a@138.68.61.31");

    // Fixed arguments for hnsd (some args are also added dynamically)
    const QStringList &hnsdFixedArgs{"-n", hnsdLocalAddress + ":1053",
                                     "-r", hnsdLocalAddress + ":53",
                                     "--seeds", hnsdSeed + "," + hnscanSeed};

    // Restart strategy for hnsd
    const RestartStrategy::Params hnsdRestart{std::chrono::milliseconds(100), // Min restart delay
                                              std::chrono::seconds(3), // Max restart delay
                                              std::chrono::seconds(30)}; // Min "successful" run time

    // Restart strategy for shadowsocks
    const RestartStrategy::Params shadowsocksRestart{std::chrono::milliseconds(100),
                                                     std::chrono::seconds(5),
                                                     std::chrono::seconds(5)};

    // Sync timeout for hnsd.  This should be shorter than the "successful" time
    // in the RestartStrategy, so the warning doesn't flap if hnsd fails for a
    // while, then starts up, but fails to sync.
    const std::chrono::seconds hnsdSyncTimeout{5};

    // Timeout for preferred transport before starting to try alternate transports
    const std::chrono::seconds preferredTransportTimeout{30};

    // All IPv4 LAN and loopback subnets
    using SubnetPair = QPair<QHostAddress, int>;
    std::array<SubnetPair, 5> ipv4LocalSubnets{
        QHostAddress::parseSubnet(QStringLiteral("192.168.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("172.16.0.0/12")),
        QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/8")),
        QHostAddress::parseSubnet(QStringLiteral("169.254.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("127.0.0.0/8"))
    };

    // Test if an address is an IPv4 LAN or loopback address
    bool isIpv4Local(const QHostAddress &addr)
    {
        return std::any_of(ipv4LocalSubnets.begin(), ipv4LocalSubnets.end(),
            [&](const SubnetPair &subnet) {return addr.isInSubnet(subnet);});
    }
}

HnsdRunner::HnsdRunner(RestartStrategy::Params restartParams)
    : ProcessRunner{std::move(restartParams)}
{
    setObjectName(QStringLiteral("hnsd"));

    _hnsdSyncTimer.setSingleShot(true);
    _hnsdSyncTimer.setInterval(msec(hnsdSyncTimeout));
    connect(&_hnsdSyncTimer, &QTimer::timeout, this,
            [this]()
            {
                // hnsd is not syncing.  Don't stop it (it continues to try to
                // connect to peers), but indicate the failure to display a UI
                // warning
                qInfo() << "hnsd has not synced any blocks, reporting error";
                emit hnsdSyncFailure(true);
            });

    connect(this, &ProcessRunner::stdoutLine, this,
            [this](const QByteArray &line)
            {
                if(line.contains(QByteArrayLiteral(" new height: ")))
                {
                    // At least 1 block has been synced, handshake is connected.
                    _hnsdSyncTimer.stop();
                    emit hnsdSyncFailure(false);

                    // We just want to show some progress of hnsd's sync in the log.
                    // Qt regexes don't work on byte arrays but this check works
                    // well enough to trace every 1000 blocks.
                    if(line.endsWith(QByteArrayLiteral("000")))
                        qInfo() << objectName() << "-" << line;
                }
            });
    // We never clear hnsdSyncFailure() when hnsd starts / stops / restarts.  We
    // try to avoid reporting it spuriously (by stopping the timer if hnsd
    // stops; the failing warning covers this state).  Once we _do_ report it
    // though, we don't want to clear it until hnsd really does sync a block, to
    // avoid a flapping warning.
    connect(this, &ProcessRunner::started, this,
            [this]()
            {
                // Start (or restart) the sync timer.
                _hnsdSyncTimer.start();
            });
    // 'succeeded' means that the process has been running for long enough that
    // ProcessRunner considers it successful.  It hasn't necessarily synced any
    // blocks yet; don't emit hnsdSyncFailure() at all.
    connect(this, &ProcessRunner::succeeded, this, &HnsdRunner::hnsdSucceeded);
    connect(this, &ProcessRunner::failed, this,
            [this](std::chrono::milliseconds failureDuration)
            {
                emit hnsdFailed(failureDuration);
                // Stop the sync timer since hnsd isn't running.  The failure
                // signal covers this state, avoid spuriously emitting a sync
                // failure too (though these states _can_ overlap so we do still
                // tolerate this in the client UI).
                _hnsdSyncTimer.stop();
            });
}

void HnsdRunner::setupProcess(UidGidProcess &process)
{
#ifdef Q_OS_LINUX
     if(hasNetBindServiceCapability())
         // Drop root privileges ("nobody" is a low-priv account that should exist on all Linux systems)
        process.setUser("nobody");
    else
        qWarning() << Path::HnsdExecutable << "did not have cap_net_bind_service set; running hnsd as root.";
#endif
#ifdef Q_OS_UNIX
    // Setting this group allows us to manage hnsd firewall rules
    process.setGroup(BRAND_CODE "hnsd");
#endif
}

bool HnsdRunner::enable(QString program, QStringList arguments)
{
#ifdef Q_OS_MACOS
    ::shellExecute(QStringLiteral("ifconfig lo0 alias %1 up").arg(::hnsdLocalAddress));

#endif
    // Invoke the original
    return ProcessRunner::enable(std::move(program), std::move(arguments));
}

void HnsdRunner::disable()
{
    // Invoke the original
    ProcessRunner::disable();

    // Not syncing or failing to sync since hnsd is no longer enabled.
    _hnsdSyncTimer.stop();
    emit hnsdSyncFailure(false);

#ifdef Q_OS_MACOS
    QByteArray out;
    std::tie(std::ignore, out, std::ignore) = ::shellExecute(QStringLiteral("ifconfig lo0"));

    // Only try to remove the alias if it exists
    if(out.contains(::hnsdLocalAddress.toLatin1()))
    {
        ::shellExecute(QStringLiteral("ifconfig lo0 -alias %1").arg(::hnsdLocalAddress));
    }
#endif
}

// Check if Hnsd can bind to low ports without requiring root
bool HnsdRunner::hasNetBindServiceCapability()
{
#ifdef Q_OS_LINUX
    int exitCode;
    QByteArray out;
    std::tie(exitCode, out, std::ignore) = ::shellExecute(QStringLiteral("getcap %1").arg(Path::HnsdExecutable));
    if(exitCode != 0)
        return false;
    else
        return out.contains("cap_net_bind_service");
#else
    return false;
#endif
}

ShadowsocksRunner::ShadowsocksRunner(RestartStrategy::Params restartParams)
    : ProcessRunner{std::move(restartParams)}, _localPort{0}
{
    connect(this, &ShadowsocksRunner::stdoutLine, this, [this](const QByteArray &line)
    {
        qInfo() << objectName() << "- stdout:" << line;

        QByteArray marker{QByteArrayLiteral("listening on TCP port ")};
        auto pos = line.indexOf(marker);
        if(pos >= 0)
        {
            _localPort = line.mid(pos + marker.length()).toUShort();
            if(_localPort)
            {
                qInfo() << objectName() << "assigned port:" << _localPort;
                emit localPortAssigned();
            }
            else
            {
                qInfo() << objectName() << "Could not detect assigned port:"
                    << line;
                // This is no good, kill the process and try again
                kill();
            }
        }
    });

    connect(this, &ShadowsocksRunner::started, this, [this]()
    {
        qInfo() << objectName() << "started, waiting for local port";
        _localPort = 0;
    });
}

bool ShadowsocksRunner::enable(QString program, QStringList arguments)
{
    if(ProcessRunner::enable(std::move(program), std::move(arguments)))
    {
        // Wipe the local port since the process is being restarted; doConnect()
        // will wait for the next process startup rather than attempting a
        // connection that will definitely fail.
        // Note that this could race with a process restart if the process is
        // crashing, so we could still observe an assigned port before the
        // process restarts.  In that case, we might still make an extra
        // connection attempt while ss-local is restarting, which is fine.
        if(_localPort)
        {
            qInfo() << objectName() << "- restarting due to parameter change, clear assigned port";
            _localPort = 0;
        }
        return true;
    }
    return false;
}

void ShadowsocksRunner::setupProcess(UidGidProcess &process)
{
#ifdef Q_OS_UNIX
    process.setUser(QStringLiteral("nobody"));
#endif
}

// Custom logging for our OriginalNetworkScan struct
QDebug operator<<(QDebug debug, const OriginalNetworkScan& netScan) {
    QDebugStateSaver saver(debug);
    debug.nospace() << QStringLiteral("Network(gatewayIp: %1, interfaceName: %2, ipAddress: %3)")
        .arg(netScan.gatewayIp(), netScan.interfaceName(), netScan.ipAddress());

    return debug;
}

TransportSelector::TransportSelector()
    : _preferred{QStringLiteral("udp"), 0}, _lastUsed{QStringLiteral("udp"), 0}, _alternates{},
      _nextAlternate{0}, _startAlternates{-1}, _status{Status::Connecting}
{
}

void TransportSelector::addAlternates(const QString &protocol,
                                      const ServerLocation &location,
                                      const QVector<uint> &ports)
{
    Transport nextTransport{protocol, 0};

    // Add the implicit default port if it's not in the list of ports
    nextTransport.resolvePort(location);
    if(_preferred != nextTransport && !ports.contains(nextTransport.port()))
        _alternates.emplace_back(nextTransport);

    for(unsigned port : ports)
    {
        nextTransport.port(port);
        if(_preferred != nextTransport)
            _alternates.emplace_back(nextTransport);
    }
}

QHostAddress TransportSelector::findInterfaceIp(const QString &interfaceName)
{
#ifdef Q_OS_UNIX
    ifreq ifr = {};
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    // We're interested in the IPv4 address
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, qPrintable(interfaceName), IFNAMSIZ - 1);

    // Request the interface address
    if(ioctl(fd, SIOCGIFADDR, &ifr))
    {
        qWarning() << "Unable to retrieve interface ip:" << qt_error_string(errno);
        close(fd);
        return {};
    }

    close(fd);

    quint32 address = reinterpret_cast<sockaddr_in*>(&ifr.ifr_addr)->sin_addr.s_addr;
    return QHostAddress{ntohl(address)};
#else
   return {};
#endif
}

QHostAddress TransportSelector::findLocalAddress(const QString &remoteAddress)
{
    // Determine what local address we will use to make this connection.
    // A "UDP connect" doesn't actually send any packets, it just determines
    // the local address to use for a connection.
    QUdpSocket connTest;
    connTest.connectToHost(remoteAddress, 8080);
    return connTest.localAddress();
}

void TransportSelector::scanNetworkRoutes(OriginalNetworkScan &netScan)
{
    QString out, err;
    QStringList result;
    int exitCode{-1};  // Default to -1 so it fails on windows for now

    // TODO: use system APIs for this rather than relying on flaky and changeable system tools (in terms of their output format)
#if defined(Q_OS_MACOS)
    // This awk script is necessary as the macOS 10.14 and 10.15 output of netstat has changed. As a result we need to actually locate the columns
    // we're interested in, we can't just hard-code a column number
    auto commandString = QStringLiteral("netstat -nr -f inet | sed '1,3 d' | awk 'NR==1 { for (i=1; i<=NF; i++) { f[$i] = i  } } NR>1 && $(f[\"Destination\"])==\"default\" { print $(f[\"Gateway\"]), $(f[\"Netif\"]) ; exit }'");
#elif defined(Q_OS_LINUX)
    auto commandString = QStringLiteral("netstat -nr | tee /dev/stderr | awk '$1==\"default\" || ($1==\"0.0.0.0\" && $3==\"0.0.0.0\") { print $2, $8; exit }'");

#endif
#ifdef Q_OS_UNIX
    std::tie(exitCode, out, err) = ::shellExecute(commandString);
    if(exitCode == 0)
    {
        result = out.split(' ');
        netScan.gatewayIp(result.first());
        netScan.interfaceName(result.last());
    }
#else
    // Not needed for Windows
    netScan.gatewayIp(QStringLiteral("N/A"));
    netScan.interfaceName(QStringLiteral("N/A"));
#endif
}

QHostAddress TransportSelector::validLastLocalAddress() const
{
    if(_lastUsed.protocol() == QStringLiteral("udp"))
        return _lastLocalUdpAddress;
    else
        return _lastLocalTcpAddress;
}

void TransportSelector::reset(Transport preferred, bool useAlternates,
                              const ServerLocation &location,
                              const QVector<uint> &udpPorts,
                              const QVector<uint> &tcpPorts)
{
    _preferred = preferred;
    _preferred.resolvePort(location);
    _alternates.clear();
    _nextAlternate = 0;
    _startAlternates.setRemainingTime(msec(preferredTransportTimeout));
    _status = Status::Connecting;
    _useAlternateNext = false;
    // Reset local addresses; doesn't really matter since we redetect them for
    // each beginAttempt()
    _lastLocalUdpAddress.clear();
    _lastLocalTcpAddress.clear();

    if(useAlternates)
    {
        // The expected count is just udpPorts.size() + tcpPorts.size().
        // There are two implicit "default" choices, but the UDP one is
        // eliminated since 8080 appears in the list (all regions use 8080 by
        // default currently).  The TCP default is 500, which is not in the
        // list, but one other possibility will be eliminated because it's the
        // preferred transport.
        _alternates.reserve(udpPorts.size() + tcpPorts.size());

        // Prefer to stay on the user's preferred protocol; try those first.
        if(_preferred.protocol() == QStringLiteral("udp"))
        {
            addAlternates(QStringLiteral("udp"), location, udpPorts);
            addAlternates(QStringLiteral("tcp"), location, tcpPorts);
        }
        else
        {
            addAlternates(QStringLiteral("tcp"), location, tcpPorts);
            addAlternates(QStringLiteral("udp"), location, udpPorts);
        }
    }
}

OriginalNetworkScan TransportSelector::scanNetwork(const ServerLocation *pLocation,
                                                   const QString &protocol)
{
    OriginalNetworkScan netScan;

    // Get the gatewayIp and interfaceName
    scanNetworkRoutes(netScan);

// The findLocalAddress() code is only broken on macOS, so let's limit the ioctl() approach to there for now.
// On macOS we add a 'bound route' (-ifscope route) that sometimes hangs around after changing network - this bound route can interfere with assigning source IPs
// to sockets and so using the findLocalAddress() approach can sometimes return the incorrect IP. Using findInterfaceIp() we can query the interface for
// its IP address which should always give us the correct result (assuming we ask the correct interface). The assumption behind using findInterfaceIp() is that
// the interface associated with the "first" default route after changing networks is added by the system and is always correct.
#ifdef Q_OS_UNIX
    netScan.ipAddress(findInterfaceIp(netScan.interfaceName()).toString());
#else
    if(pLocation)
    {
        if(protocol == QStringLiteral("udp"))
            netScan.ipAddress(findLocalAddress(pLocation->udpHost()).toString());
        else
            netScan.ipAddress(findLocalAddress(pLocation->tcpHost()).toString());
    }
#endif
    return netScan;
}

QHostAddress TransportSelector::lastLocalAddress() const
{
    // If the last transport is the preferred transport, always allow any local
    // address, even if we have alternate transport configurations to try.
    //
    // This ensures that we behave the same as prior releases most of the time,
    // either when "Try Alternate Transports" is turned off or for connections
    // using the preferred transport setting when it is on.
    if(_lastUsed == _preferred)
        return {};

    // When using an alternate transport, restrict to the last local address we
    // found.  If the network connection changes, we don't want an alternate
    // transport to succeed by chance, we want it to fail so we can try the
    // preferred transport again.
    return validLastLocalAddress();
}

bool TransportSelector::beginAttempt(const ServerLocation &location,
                                     OriginalNetworkScan &netScan)
{
    scanNetworkRoutes(netScan);

    // Find the local addresses that we would use to connect to either the
    // TCP or UDP addresses for this location.  If they change, we reset and go
    // back to the preferred transport only.
#ifdef Q_OS_MACOS
    QHostAddress localUdpAddress = findInterfaceIp(netScan.interfaceName());
    // Source IP is the same regardless of protocol
    QHostAddress localTcpAddress = localUdpAddress;
#else
    QHostAddress localUdpAddress = findLocalAddress(location.udpHost());
    QHostAddress localTcpAddress = findLocalAddress(location.tcpHost());
#endif

    netScan.ipAddress({});

    // If either address has changed, a network connectivity change has
    // occurred.  Reset and try the preferred transport only for a while.
    //
    // If Try Alternate Transports is turned off, this has no effect, because we
    // don't have any alternate transports to try in that case.
    if(localUdpAddress != _lastLocalUdpAddress || localTcpAddress != _lastLocalTcpAddress)
    {
        qInfo() << "UDP:" << localUdpAddress << "->" << location.udpHost();
        qInfo() << "TCP:" << localTcpAddress << "->" << location.tcpHost();
        qInfo() << "Network connectivity has changed since last attempt, start over from preferred transport";
        _lastLocalUdpAddress = localUdpAddress;
        _lastLocalTcpAddress = localTcpAddress;
        _nextAlternate = 0;
        _startAlternates.setRemainingTime(msec(preferredTransportTimeout));
        _status = Status::Connecting;
        _useAlternateNext = false;
    }

    // Advance status when the alternate timer elapses
    if(_status == Status::Connecting && _startAlternates.hasExpired())
    {
        if(_alternates.empty() || _lastLocalUdpAddress.isNull() ||
           _lastLocalTcpAddress.isNull())
        {
            // Can't try alternates - they're not enabled, or we weren't able to
            // detect a local address (we probably aren't connected right now).
            _status = Status::TroubleConnecting;
        }
        else
        {
            _status = Status::TryingAlternates;
        }
    }

    bool delayNext = true;

    // Always use the preferred transport if:
    // - there are no alternates
    // - the preferred transport interval hasn't elapsed (still in Connecting)
    // - we failed to detect a local IP address for the connection (this means
    //   we are not connected to a network right now, and we don't want to
    //   attempt an alternate transport with "any" local address)
    if(_alternates.empty() || _status == Status::Connecting ||
        _lastLocalUdpAddress.isNull() || _lastLocalTcpAddress.isNull())
    {
        _lastUsed = _preferred;
    }
    // After a few failures, start trying alternates.  After each retry delay,
    // we try the preferred settings, then immediately try one alternate if that
    // still fails (to minimize the possibility that the timing of network
    // connections, etc. could cause us to fail over spuriously).
    //
    // To do that, alternate between the preferred settings with no delay, and
    // the next alternate with the usual delay.
    else if(!_useAlternateNext)
    {
        // Try preferred settings.
        _useAlternateNext = true;
        _lastUsed = _preferred;
        // No delay since we'll try an alternate next.
        delayNext = false;
    }
    else
    {
        // Try the next alternate
        if(_nextAlternate >= _alternates.size())
            _nextAlternate = 0;
        _lastUsed = _alternates[_nextAlternate];
        ++_nextAlternate;
        _useAlternateNext = false;
    }

    netScan.ipAddress(validLastLocalAddress().toString());

    return delayNext;
}

QHostAddress ConnectionConfig::parseIpv4Host(const QString &host)
{
    // The proxy address must be a literal IPv4 address, we cannot
    // resolve hostnames due DNS being blocked during reconnection.  For
    // any failure, leave the resulting QHostAddress clear.
    QHostAddress socksTestAddress;
    if(socksTestAddress.setAddress(host))
    {
        // Only IPv4 is supported currently
        if(socksTestAddress.protocol() != QAbstractSocket::NetworkLayerProtocol::IPv4Protocol)
        {
            qWarning() << "Invalid SOCKS proxy network protocol"
                << socksTestAddress.protocol() << "for address"
                << socksTestAddress << "- parsed from" << host;
        }
    }
    else
    {
        qInfo() << "Invalid SOCKS proxy address:" << host;
    }

    return socksTestAddress;
}

ConnectionConfig::ConnectionConfig()
    : _vpnLocationAuto{false}, _defaultRoute{true},
      _proxyType{ProxyType::None}, _shadowsocksLocationAuto{false}
{}

ConnectionConfig::ConnectionConfig(DaemonSettings &settings, DaemonState &state)
    : ConnectionConfig{}
{
    // Grab the next VPN location.  Copy it in case the locations in DaemonState
    // are updated.
    if(state.vpnLocations().nextLocation())
        _pVpnLocation.reset(new ServerLocation{*state.vpnLocations().nextLocation()});
    _vpnLocationAuto = !state.vpnLocations().chosenLocation();

    // If split tunnel is not enabled, we always use the VPN for the default
    // route - only apply the defaultRoute() setting if splitTunnelEnabled() is
    // set
    if(settings.splitTunnelEnabled())
        _defaultRoute = settings.defaultRoute();

    if(settings.proxy() == QStringLiteral("custom"))
    {
        _proxyType = ProxyType::Custom;
        _customProxy = settings.proxyCustom();

        // The proxy address must be a literal IPv4 address, we cannot resolve
        // hostnames due DNS being blocked during reconnection.  For any
        // failure, leave _socksHostAddress clear, which will be detected as a
        // nonfatal error later.
        _socksHostAddress = parseIpv4Host(_customProxy.host());
    }
    else if(settings.proxy() == QStringLiteral("shadowsocks"))
    {
        _proxyType = ProxyType::Shadowsocks;
        _socksHostAddress = QHostAddress{0x7f000001}; // 127.0.0.1
        if(state.shadowsocksLocations().nextLocation())
            _pShadowsocksLocation.reset(new ServerLocation{*state.shadowsocksLocations().nextLocation()});
        _shadowsocksLocationAuto = !state.shadowsocksLocations().chosenLocation();
    }
}

bool ConnectionConfig::canConnect() const
{
    if(!vpnLocation())
    {
        qWarning() << "No VPN location found, cannot connect";
        return false;   // Always required for any connection
    }

    switch(proxyType())
    {
        default:
        case ProxyType::None:
            break;
        case ProxyType::Shadowsocks:
            if(!shadowsocksLocation() || !shadowsocksLocation()->shadowsocks())
            {
                qWarning() << "No Shadowsocks location found when using Shadowsocks proxy, cannot connect";
                return false;
            }
            // Invariant; always 127.0.0.1 in this state
            Q_ASSERT(!socksHost().isNull());
            break;
        case ProxyType::Custom:
            if(socksHost().isNull())
            {
                qWarning() << "SOCKS5 host invalid when using SOCKS5 proxy, cannot connect";
                return false;
            }
            break;
    }

    return true;
}

bool ConnectionConfig::hasChanged(const ConnectionConfig &other) const
{
    // Only consider location changes if the location ID has changed.  Ignore
    // changes in latency, metadata, etc.; we don't need to reset for these.
    QString vpnLocationId = vpnLocation() ? vpnLocation()->id() : QString{};
    QString ssLocationId = shadowsocksLocation() ? shadowsocksLocation()->id() : QString{};
    QString otherVpnLocationId = other.vpnLocation() ? other.vpnLocation()->id() : QString{};
    QString otherSsLocationId = other.shadowsocksLocation() ? other.shadowsocksLocation()->id() : QString{};

    return defaultRoute() != other.defaultRoute() ||
        proxyType() != other.proxyType() ||
        socksHost() != other.socksHost() ||
        customProxy() != other.customProxy() ||
        vpnLocationId != otherVpnLocationId ||
        ssLocationId != otherSsLocationId;
}

VPNConnection::VPNConnection(QObject* parent)
    : QObject(parent)
    , _state(State::Disconnected)
    , _connectionStep{ConnectionStep::Initializing}
    , _openvpn(nullptr)
    , _hnsdRunner{hnsdRestart}
    , _shadowsocksRunner{shadowsocksRestart}
    , _connectionAttemptCount(0)
    , _receivedByteCount(0)
    , _sentByteCount(0)
    , _lastReceivedByteCount(0)
    , _lastSentByteCount(0)
    , _needsReconnect(false)
{
    _shadowsocksRunner.setObjectName("shadowsocks");

    _connectTimer.setSingleShot(true);
    connect(&_connectTimer, &QTimer::timeout, this, &VPNConnection::beginConnection);

    connect(&_hnsdRunner, &HnsdRunner::hnsdSucceeded, this, &VPNConnection::hnsdSucceeded);
    connect(&_hnsdRunner, &HnsdRunner::hnsdFailed, this, &VPNConnection::hnsdFailed);
    connect(&_hnsdRunner, &HnsdRunner::hnsdSyncFailure, this, &VPNConnection::hnsdSyncFailure);

    // The succeeded/failed signals from _shadowsocksRunner are ignored.  It
    // rarely fails, particularly since it does not do much of anything until we
    // try to make a connection through it.  Most failures would be covered by
    // the general proxy diagnostics anyway (failed to connect, etc.)  The only
    // unique failure mode is if ss-local crashes while connected, this will
    // give a general connection failure but potentially could give a specific
    // error.
    connect(&_shadowsocksRunner, &ShadowsocksRunner::localPortAssigned, this, [this]()
    {
        // This signal happens any time Shadowsocks is restarted, including if
        // it crashed while we were connected.
        // We only need to invoke doConnect() if we were specifically waiting on
        // this port to be assigned, otherwise, let the connection drop and
        // retry normally.
        if((_state == State::Connecting || _state == State::Reconnecting ||
            _state == State::StillConnecting || _state == State::StillReconnecting) &&
           _connectionStep == ConnectionStep::StartingProxy)
        {
            qInfo() << "Shadowsocks proxy assigned local port"
                << _shadowsocksRunner.localPort() << "- continue connecting";
            doConnect();
        }
        else
        {
            qWarning() << "Shadowsocks proxy assigned local port"
                << _shadowsocksRunner.localPort()
                << "but we were not waiting on it to connect";
        }
    });
}

void VPNConnection::activateMACE()
{
    // To activate MACE, we need to send a TCP packet to 209.222.18.222 port 1111.
    // This sets the DNS servers to block queries for known tracking domains
    // As a fallback, we also need to send another packet 5 seconds later
    // (see pia_manager's openvpn_manager.rb for original implementation)
    //
    // We can later test if we need a second packet, but it might be used to mitigate
    // issues occured by sending a packet immediately.
    auto maceActivate1 = new QTcpSocket();
    qDebug () << "Sending MACE Packet 1";
    maceActivate1->connectToHost(QStringLiteral("209.222.18.222"), 1111);
    // Tested if error condition is hit by changing the port/invalid IP
    connect(maceActivate1,QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this,
            [maceActivate1](QAbstractSocket::SocketError socketError) {
        qError() << "Failed to activate MACE packet 1 " << socketError;
        maceActivate1->deleteLater();
    });
    connect(maceActivate1, &QAbstractSocket::connected, this, [maceActivate1]() {
        qDebug () << "MACE Packet 1 connected succesfully";
        maceActivate1->close();
        maceActivate1->deleteLater();
    });

    QTimer::singleShot(std::chrono::milliseconds(5000).count(), this, [this]() {
        auto maceActivate2 = new QTcpSocket();
        qDebug () << "Sending MACE Packet 2";
        maceActivate2->connectToHost(QStringLiteral("209.222.18.222"), 1111);
        connect(maceActivate2,QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this,
            [maceActivate2](QAbstractSocket::SocketError socketError) {
        qError() << "Failed to activate MACE packet 2 " << socketError;
        maceActivate2->deleteLater();
        });
        connect(maceActivate2, &QAbstractSocket::connected, this, [maceActivate2]() {
        qDebug () << "MACE Packet 2 connected succesfully";
        maceActivate2->close();
        maceActivate2->deleteLater();
        });
    });
}

bool VPNConnection::needsReconnect()
{
    if (!_openvpn || _state == State::Disconnecting || _state == State::DisconnectingToReconnect || _state == State::Disconnected)
        return _needsReconnect = false;
    if (_needsReconnect)
        return true;
    for (auto name : g_connectionSettingNames)
    {
        QJsonValue storedValue = _connectionSettings.value(QLatin1String(name));
        QJsonValue currentValue = g_settings.get(name);
        if (storedValue != currentValue)
            return _needsReconnect = true;
    }
    ConnectionConfig newConfig{g_settings, g_state};
    switch(_state)
    {
    default:
    case State::Disconnected:
    case State::Disconnecting:
    case State::DisconnectingToReconnect:
        // Not possible; checked for these states above
        Q_ASSERT(false);
        break;
    case State::Connecting:
    case State::StillConnecting:
    case State::Reconnecting:
    case State::StillReconnecting:
    case State::Interrupted:
        // Compare the current settings to the settings we are currently trying to use
        if(newConfig.hasChanged(_connectingConfig))
        {
            qInfo() << "Reconnect needed in" << traceEnum(_state)
                << "state due to change in connection configuration";
            _needsReconnect = true;
        }
        break;
    case State::Connected:
        // Compare the current settings to the settings we are connected with
        if(newConfig.hasChanged(_connectedConfig))
        {
            qInfo() << "Reconnect needed in" << traceEnum(_state)
                << "state due to change in connection configuration";
            _needsReconnect = true;
        }
        break;
    }
    return _needsReconnect;
}

void VPNConnection::scanNetwork(const ServerLocation *pLocation, const QString &protocol)
{
    emit scannedOriginalNetwork(_transportSelector.scanNetwork(pLocation, protocol));
}

void VPNConnection::connectVPN(bool force)
{
    switch (_state)
    {
    case State::Connected:
        Q_ASSERT(_connectedConfig.vpnLocation()); // Valid in this state
        // If settings haven't changed, there's nothing to do.
        if (!force && !needsReconnect())
            return;

        // Otherwise, change to DisconnectingToReconnect
        copySettings(State::DisconnectingToReconnect, State::Disconnecting);

        Q_ASSERT(_openvpn); // Valid in this state
        _openvpn->shutdown();
        return;
    case State::Connecting:
    case State::StillConnecting:
    case State::Reconnecting:
    case State::StillReconnecting:
        // If settings haven't changed, there's nothing to do.
        if (!force && !needsReconnect())
            return;
        // fallthrough
        if (_openvpn)
        {
            _openvpn->shutdown();
            copySettings(State::DisconnectingToReconnect, State::Disconnecting);
        }
        else
        {
            // Don't go to the DisconnectingToReconnect state since we weren't
            // actively attempting a connection.  Instead, go directly to the
            // Connecting or Reconnecting state.
            auto newState = _connectedConfig.vpnLocation() ? State::Reconnecting : State::Connecting;
            copySettings(newState, State::Disconnected);
            // We already queued a connection attempt when entering the
            // Interrupted state; nothing else to do.
        }
        return;
    case State::DisconnectingToReconnect:
        // If we're already disconnecting to reconnect, there's nothing to do.
        // We'll reconnect as planned with the new settings when OpenVPN exits.
        return;
    default:
        qWarning() << "Connecting in unhandled state " << _state;
        // fallthrough
    case State::Disconnected:
        _connectionAttemptCount = 0;
        _connectedConfig = {};
        if(copySettings(State::Connecting, State::Disconnected))
            queueConnectionAttempt();
        return;
    }
}

void VPNConnection::disconnectVPN()
{
    if (_state != State::Disconnected)
    {
        _connectingConfig = {};
        setState(State::Disconnecting);
        if (_openvpn && _openvpn->state() < OpenVPNProcess::Exiting)
            _openvpn->shutdown();
        if (!_openvpn || _openvpn->state() == OpenVPNProcess::Exited)
        {
            setState(State::Disconnected);
        }
    }
}

void VPNConnection::beginConnection()
{
    _connectionStep = ConnectionStep::Initializing;
    doConnect();
}

void VPNConnection::doConnect()
{
    switch (_state)
    {
    case State::Interrupted:
        setState(State::Reconnecting);
        break;
    case State::DisconnectingToReconnect:
        // Will transition to Reconnecting when the disconnect completes.
        qInfo() << "Currently disconnecting to reconnect; doConnect ignored";
        return;
    case State::Connecting:
    case State::Reconnecting:
    case State::StillConnecting:
    case State::StillReconnecting:
        break;
    case State::Connected:
        qInfo() << "Already connected; doConnect ignored";
        return;
    case State::Disconnected:
    case State::Disconnecting:
        // 'Disconnected' and 'Disconnecting' should have transitioned where the
        // call to doConnect was scheduled (normally connectVPN()), we should
        // only observe these here if disconnectVPN() was then called before the
        // deferred call to doConnect().
        qInfo() << "doConnect() called after disconnect, ignored";
        return;
    default:
        qError() << "Called doConnect in unknown state" << qEnumToString(_state);
        return;
    }

    if (_openvpn)
    {
        qWarning() << "OpenVPN process already exists; doConnect ignored";
        return;
    }

    // Handle pre-connection steps.  Note that these _cannot_ fail with nonfatal
    // errors - we need to apply the failure logic later to set the next request
    // delay and possibly change state.  Nonfatal errors have to be detected
    // later when we're about to start OpenVPN.
    if(_connectionStep == ConnectionStep::Initializing)
    {
        // Copy settings to begin the attempt (may reset the attempt count)
        if(!copySettings(_state, State::Disconnected))
        {
            // Failed to load locations, already traced by copySettings, just
            // bail now that we are in the Disconnected state
            return;
        }
        // Consequence of copySettings(), required below
        Q_ASSERT(_connectingConfig.vpnLocation());

        _connectionStep = ConnectionStep::FetchingIP;
        // Do we need to fetch the non-VPN IP address?  Do this for the first
        // connection attempt (which resets if the network connection changes).
        // However, we can't do it at all if we're reconnecting, because the
        // killswitch blocks DNS resolution.
        if(_connectionAttemptCount == 0 &&
           (_state == State::Connecting || _state == State::StillConnecting))
        {
            // We're not retrying this request if it fails - we don't want to hold
            // up the connection attempt; this information isn't critical.
            ApiClient::instance()
                    ->getIp(QStringLiteral("status"))
                    ->notify(this, [this](const Error& error, const QJsonDocument& json) {
                        if (!error)
                        {
                            QString ip = json[QStringLiteral("ip")].toString();
                            if (!ip.isEmpty())
                            {
                                g_state.externalIp(ip);
                            }
                        }
                        doConnect();
                    }, Qt::QueuedConnection); // Deliver results asynchronously so we never recurse
            return;
        }
    }

    // We either finished fetching the IP or we skipped it.  Do we need to
    // start a proxy?
    if(_connectionStep == ConnectionStep::FetchingIP)
    {
        _connectionStep = ConnectionStep::StartingProxy;
        if(_connectingConfig.shadowsocksLocation() && _connectingConfig.shadowsocksLocation()->shadowsocks())
        {
            const auto &pSsServer = _connectingConfig.shadowsocksLocation()->shadowsocks();
            _shadowsocksRunner.enable(Path::SsLocalExecutable,
                QStringList{QStringLiteral("-s"), pSsServer->host(),
                            QStringLiteral("-p"), QString::number(pSsServer->port()),
                            QStringLiteral("-k"), pSsServer->key(),
                            QStringLiteral("-b"), QStringLiteral("127.0.0.1"),
                            QStringLiteral("-l"), QStringLiteral("0"),
                            QStringLiteral("-m"), pSsServer->cipher()});

            // If we don't already know a listening port, wait for it to tell
            // us (we could already know if the SS client was already running)
            if(_shadowsocksRunner.localPort() == 0)
            {
                qInfo() << "Wait for local proxy port to be assigned";
                return;
            }
            else
            {
                qInfo() << "Local proxy has already assigned port"
                    << _shadowsocksRunner.localPort();
            }
        }
        else
            _shadowsocksRunner.disable();
    }

    // We either finished starting a proxy or we skipped it.  We're ready to connect
    Q_ASSERT(_connectionStep == ConnectionStep::StartingProxy);
    _connectionStep = ConnectionStep::ConnectingOpenVPN;

    if (_connectionAttemptCount == 0)
    {
        // We shouldn't have any problems json_cast()ing these values since they
        // came from DaemonSettings; just use defaults if it does happen
        // somehow.
        QString protocol;
        uint selectedPort;
        bool automaticTransport;
        if(!json_cast(_connectionSettings.value("protocol"), protocol))
            protocol = QStringLiteral("udp");
        // Shadowsocks proxies require TCP; UDP relays aren't enabled on PIA
        // servers.
        if(_connectingConfig.proxyType() == ConnectionConfig::ProxyType::Shadowsocks &&
            protocol != QStringLiteral("tcp"))
        {
            qInfo() << "Using TCP transport due to Shadowsocks proxy setting";
            protocol = QStringLiteral("tcp");
        }

        if(protocol == QStringLiteral("udp"))
        {
            if(!json_cast(_connectionSettings.value("remotePortUDP"), selectedPort))
                selectedPort = 0;
        }
        else
        {
            if(!json_cast(_connectionSettings.value("remotePortTCP"), selectedPort))
                selectedPort = 0;
        }

        if(!json_cast(_connectionSettings.value("automaticTransport"), automaticTransport))
            automaticTransport = true;

        // Automatic transport isn't supported when a SOCKS proxy is also
        // configured.  (We can't be confident that a failure to connect is due
        // to a port being blocked.)
        if(_connectingConfig.proxyType() != ConnectionConfig::ProxyType::None)
            automaticTransport = false;

        // Reset the transport selection sequence
        _transportSelector.reset({protocol, selectedPort}, automaticTransport,
                                 *_connectingConfig.vpnLocation(),
                                 g_data.udpPorts(), g_data.tcpPorts());
    }

    // Reset traffic counters since we have a new process
    _lastReceivedByteCount = 0;
    _lastSentByteCount = 0;
    _intervalMeasurements.clear();
    emit byteCountsChanged();

    // Reset any running connect timer, just in case
    _connectTimer.stop();

    OriginalNetworkScan netScan;
    bool delayNext = _transportSelector.beginAttempt(*_connectingConfig.vpnLocation(), netScan);
    // Emit the current network configuration, so it can be used for split
    // tunnel if it's known.  If we did find it (and it doesn't change by the
    // time we connect), this avoids a blip for excluded apps where they might
    // route into the VPN tunnel once OpenVPN sets up the routes.
    //
    // This scan is not reliable though - the network could come up or change
    // between now and when OpenVPN starts.  We do another scan just after
    // connecting to be sure that we get the correct config.  (If those scans
    // differ, there may be a connectivity blip, but the network connections
    // changed so a blip is OK.)
    emit scannedOriginalNetwork(netScan);

    // Set when the next earliest reconnect attempt is allowed
    if(delayNext)
    {
        switch (_state)
        {
        case State::Connecting:
            _timeUntilNextConnectionAttempt.setRemainingTime(ConnectionAttemptInterval);
            break;
        case State::StillConnecting:
            _timeUntilNextConnectionAttempt.setRemainingTime(SlowConnectionAttemptInterval);
            break;
        case State::Reconnecting:
            _timeUntilNextConnectionAttempt.setRemainingTime(ReconnectionAttemptInterval);
            break;
        case State::StillReconnecting:
            _timeUntilNextConnectionAttempt.setRemainingTime(SlowReconnectionAttemptInterval);
            break;
        default:
            break;
        }
    }
    else
        _timeUntilNextConnectionAttempt.setRemainingTime(0);

    ++_connectionAttemptCount;

    if (_state == State::Connecting && _connectionAttemptCount > SlowConnectionAttemptLimit)
        setState(State::StillConnecting);
    else if (_state == State::Reconnecting && _connectionAttemptCount > SlowReconnectionAttemptLimit)
        setState(State::StillReconnecting);

    emit connectingStatus(_transportSelector.status());

    QStringList arguments;
    try
    {
        arguments += QStringLiteral("--verb");
        arguments += QStringLiteral("4");

        _networkAdapter = g_daemon->getNetworkAdapter();
        if (_networkAdapter)
        {
            QString devNode = _networkAdapter->devNode();
            if (!devNode.isEmpty())
            {
                arguments += QStringLiteral("--dev-node");
                arguments += devNode;
            }
        }

        arguments += QStringLiteral("--script-security");
        arguments += QStringLiteral("2");

        auto escapeArg = [](QString arg)
        {
            // Escape backslashes and spaces in the command.  Note this is the
            // same even on Windows, because OpenVPN parses this command line
            // with its internal parser, then re-joins and re-quotes it on
            // Windows with Windows quoting conventions.
            arg.replace(QLatin1String(R"(\)"), QLatin1String(R"(\\)"));
            arg.replace(QLatin1String(R"( )"), QLatin1String(R"(\ )"));
            return arg;
        };

        QString updownCmd;

#ifdef Q_OS_WIN
        updownCmd += escapeArg(QString::fromLocal8Bit(qgetenv("WINDIR")) + "\\system32\\cmd.exe");
        updownCmd += QStringLiteral(" /C call ");
#endif
        updownCmd += escapeArg(Path::OpenVPNUpDownScript);

#ifdef Q_OS_WIN
        // Only the Windows updown script supports logging right now.  Enable it
        // if debug logging is turned on.
        if(g_settings.debugLogging())
        {
            updownCmd += " --log ";
            updownCmd += escapeArg(Path::UpdownLogFile);
        }
#endif

        // Pass DNS server addresses
        QStringList dnsServers = getDNSServers(_dnsServers);
        if(!dnsServers.isEmpty())
        {
            updownCmd += " --dns ";
            updownCmd += dnsServers.join(':');
        }

        // Terminate PIA args with '--' (OpenVPN passes several subsequent
        // arguments)
        updownCmd += " --";

        qInfo() << "updownCmd is: " << updownCmd;

#ifdef Q_OS_WIN
        if(g_settings.windowsIpMethod() == QStringLiteral("static"))
        {
            // Static configuration on Windows - use OpenVPN's netsh method, use
            // updown script to apply DNS with netsh
            arguments += "--ip-win32";
            arguments += "netsh";
            // Use the same script for --up and --down
            arguments += "--up";
            arguments += updownCmd;
            arguments += "--down";
            arguments += updownCmd;
        }
        else
        {
            // DHCP configuration on Windows - use OpenVPN's default ip-win32
            // method, pass DNS servers as DHCP options
            for(const auto &dnsServer : dnsServers)
            {
                arguments += "--dhcp-option";
                arguments += "DNS";
                arguments += dnsServer;
            }
        }
#else
        // Mac and Linux - always use updown script for DNS
        // Use the same script for --up and --down
        arguments += "--up";
        arguments += updownCmd;
        arguments += "--down";
        arguments += updownCmd;
#endif

        arguments += QStringLiteral("--config");

        QFile configFile(Path::OpenVPNConfigFile);
        if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text) ||
            !writeOpenVPNConfig(configFile))
        {
            throw Error(HERE, Error::OpenVPNConfigFileWriteError);
        }
        configFile.close();

        arguments += Path::OpenVPNConfigFile;
    }
    catch (const Error& ex)
    {
        // Do _not_ call raiseError() here because it would immediately schedule
        // another connection attempt for nonfatal errors (we haven't created
        // _openvpn yet).  Schedule the next attempt so we back off the retry
        // interval.  There are no fatal errors that can occur here (the only
        // fatal error is an auth error, which is signaled by OpenVPN after it's
        // started).
        emit error(ex);
        scheduleNextConnectionAttempt();
        return;
    }

    if (_networkAdapter)
    {
        // Ensure our tunnel has priority over other interfaces. This is especially important for DNS.
        _networkAdapter->setMetricToLowest();
    }

    _openvpn = new OpenVPNProcess(this);

    // TODO: this can be hooked up to support in-process up/down script handling via scripts that print magic strings
    connect(_openvpn, &OpenVPNProcess::stdoutLine, this, &VPNConnection::openvpnStdoutLine);
    connect(_openvpn, &OpenVPNProcess::stderrLine, this, &VPNConnection::openvpnStderrLine);
    connect(_openvpn, &OpenVPNProcess::managementLine, this, &VPNConnection::openvpnManagementLine);
    connect(_openvpn, &OpenVPNProcess::stateChanged, this, &VPNConnection::openvpnStateChanged);
    connect(_openvpn, &OpenVPNProcess::exited, this, &VPNConnection::openvpnExited);
    connect(_openvpn, &OpenVPNProcess::error, this, &VPNConnection::openvpnError);

    _openvpn->run(arguments);
}

void VPNConnection::openvpnStdoutLine(const QString& line)
{
    FUNCTION_LOGGING_CATEGORY("openvpn.stdout");
    qDebug().noquote() << line;

    checkStdoutErrors(line);
}

void VPNConnection::checkStdoutErrors(const QString &line)
{
    // Check for specific errors that we can detect from OpenVPN's output.

    if(line.contains("socks_username_password_auth: server refused the authentication"))
    {
        raiseError({HERE, Error::Code::OpenVPNProxyAuthenticationError});
        return;
    }

    // This error can be logged by socks_handshake and recv_socks_reply, others
    // might be possible too.
    if(line.contains(QRegExp("socks.*: TCP port read failed")))
    {
        raiseError({HERE, Error::Code::OpenVPNProxyError});
        return;
    }

    // This error occurs if OpenVPN fails to open a TCP connection.  If it's
    // reported for the SOCKS proxy, report it as a proxy error, otherwise let
    // it be handled as a general failure.
    QRegExp tcpFailRegex{R"(TCP: connect to \[AF_INET\]([\d\.]+):\d+ failed:)"};
    if(line.contains(tcpFailRegex))
    {
        QHostAddress failedHost;
        if(failedHost.setAddress(tcpFailRegex.cap(1)) &&
            failedHost == _connectingConfig.socksHost())
        {
            raiseError({HERE, Error::Code::OpenVPNProxyError});
            return;
        }
    }
}

void VPNConnection::openvpnStderrLine(const QString& line)
{
    FUNCTION_LOGGING_CATEGORY("openvpn.stderr");
    qDebug().noquote() << line;

    checkForMagicStrings(line);
}

void VPNConnection::checkForMagicStrings(const QString& line)
{
    QRegExp tunDeviceNameRegex{R"(Using device:([^ ]+) local_address:([^ ]+) remote_address:([^ ]+))"};
    if(line.contains(tunDeviceNameRegex))
    {
        emit usingTunnelDevice(tunDeviceNameRegex.cap(1), tunDeviceNameRegex.cap(2), tunDeviceNameRegex.cap(3));
    }

    // TODO: extract this out into a more general error mechanism, where the "!!!" prefix
    // indicates an error condition followed by the code.
    if (line.startsWith("!!!updown.sh!!!dnsConfigFailure")) {
        raiseError(Error(HERE, Error::OpenVPNDNSConfigError));
    }
}

bool VPNConnection::respondToMgmtAuth(const QString &line, const QString &user,
                                      const QString &password)
{
    // Extract the auth type from the prompt - such as `Auth` or `SOCKS Proxy`.
    // The line looks like:
    // >PASSWORD:Need 'Auth' username/password
    auto quotes = line.midRef(15).split('\'');
    if (quotes.size() >= 3)
    {
        auto id = quotes[1].toString();
        auto cmd = QStringLiteral("username \"%1\" \"%2\"\npassword \"%1\" \"%3\"")
                .arg(id, user, password);
        _openvpn->sendManagementCommand(QLatin1String(cmd.toLatin1()));
        return true;
    }

    qError() << "Invalid password request";
    return false;
}

void VPNConnection::openvpnManagementLine(const QString& line)
{
    FUNCTION_LOGGING_CATEGORY("openvpn.mgmt");
    qDebug().noquote() << line;

    if (!line.isEmpty() && line[0] == '>')
    {
        if (line.startsWith(QLatin1String(">PASSWORD:")))
        {
            // SOCKS proxy auth
            if (line.startsWith(QLatin1String(">PASSWORD:Need 'SOCKS Proxy'")))
            {
                if(respondToMgmtAuth(line, _connectingConfig.customProxy().username(),
                                     _connectingConfig.customProxy().password()))
                {
                    return;
                }
            }
            // Normal password authentication
            // The type is usually 'Auth', but use these creds by default to
            // preserve existing behavior.
            else if (line.startsWith(QLatin1String(">PASSWORD:Need ")))
            {
                if(respondToMgmtAuth(line, _openvpnUsername, _openvpnPassword))
                    return;
            }
            else if (line.startsWith(QLatin1String(">PASSWORD:Auth-Token:")))
            {
                // TODO: PIA servers aren't set up to use this properly yet
                return;
            }
            else if (line.startsWith(QLatin1String(">PASSWORD:Verification Failed: ")))
            {
                raiseError(Error(HERE, Error::OpenVPNAuthenticationError));
                return;
            }
            // All unhandled cases
            raiseError(Error(HERE, Error::OpenVPNAuthenticationError));
        }
        else if (line.startsWith(QLatin1String(">BYTECOUNT:")))
        {
            auto params = line.midRef(11).split(',');
            if (params.size() >= 2)
            {
                bool ok = false;
                quint64 up, down;
                if (((down = params[0].toULongLong(&ok)), ok) && ((up = params[1].toULongLong(&ok)), ok))
                {
                    updateByteCounts(down, up);
                }
            }
        }
    }
}

void VPNConnection::openvpnStateChanged()
{
    OpenVPNProcess::State openvpnState = _openvpn ? _openvpn->state() : OpenVPNProcess::Exited;
    State newState = _state;

    switch (openvpnState)
    {
    case OpenVPNProcess::AssignIP:
#if defined(Q_OS_WIN)
        // On Windows, we can't easily get environment variables from the
        // updown script like other platforms - stdout/stderr are not forwarded
        // through OpenVPN - so we can't capture the tunnel configuration that
        // way.
        //
        // Right now we only need the local tunnel interface IP on Windows (we
        // don't need the remote or interface name), so we can get that from
        // the AssignIP state information.  Only do this on Windows so the other
        // platforms still report all configuration components together.
        if(!_openvpn || _openvpn->tunnelIP().isEmpty())
        {
            qWarning() << "Tunnel interface IP address not known in AssignIP state";
        }
        else
        {
            qInfo() << "Tunnel device is assigned IP address" << _openvpn->tunnelIP();
            emit usingTunnelDevice({}, _openvpn->tunnelIP(), {});
        }
#endif
        break;
    case OpenVPNProcess::Connected:
        switch (_state)
        {
        default:
            qWarning() << "OpenVPN connected in unexpected state" << qEnumToString(_state);
            Q_FALLTHROUGH();
        case State::Connecting:
        case State::StillConnecting:
        case State::Reconnecting:
        case State::StillReconnecting:
            // The connection was established, so the connecting location is now
            // the connected location.
            _connectedConfig = std::move(_connectingConfig);
            _connectingConfig = {};

            // Do a new network scan now that we've connected.  If split tunnel
            // is enabled (now or later while connected), this is necessary to
            // ensure that we have the correct local IP address.  If the network
            // configuration changes at any point after this (or even if it had
            // already changed since the connection was established), we'll drop
            // the OpenVPN connection and re-scan when we connect again.
            scanNetwork(_connectedConfig.vpnLocation().get(), _transportSelector.lastUsed().protocol());

            // If DNS is set to Handshake, start it now, since we've connected
            if(isDNSHandshake(_dnsServers))
            {
                auto hnsdArgs = hnsdFixedArgs;
                // Tell hnsd to use the VPN interface for outgoing DNS queries.
                //
                // This matters when defaultRoute is off - split tunnel has
                // issues with UDP that are being addressed separately, so for
                // now hnsd is patched to handle this.  (We still need to treat
                // hnsd as a VPN-only app to handle its TCP connections to
                // Handshake nodes.)
                //
                // Also provide 127.0.0.1 as an outgoing interface since the
                // authoritative root server (part of hnsd itself) is on
                // localhost - this relies on patches applied to libunbound to
                // use loopback interfaces for loopback queries only.
                hnsdArgs.push_back(QStringLiteral("--outgoing-dns-if"));
                hnsdArgs.push_back(QStringLiteral("127.0.0.1,") + g_state.tunnelDeviceLocalAddress());
                _hnsdRunner.enable(Path::HnsdExecutable, hnsdArgs);
            }

            newState = State::Connected;
            break;
        case State::Disconnecting:
        case State::Disconnected:
        case State::DisconnectingToReconnect:
            // In these cases, we should have already told it to shutdown, and
            // the connection raced with the shutdown.  Just in case, tell it to
            // shutdown again; can't hurt anything and ensures that we don't get
            // stuck in this state.
            if (_openvpn)
                _openvpn->shutdown();
            break;
        }
        break;

    case OpenVPNProcess::Reconnecting:
        // In some rare cases OpenVPN reconnects on its own (e.g. TAP adapter I/O failure),
        // but we don't want it to try to reconnect on its own; send a SIGTERM instead.
        qWarning() << "OpenVPN trying to reconnect internally, sending SIGTERM";
        if (_openvpn)
            _openvpn->shutdown();
        break;

    case OpenVPNProcess::Exiting:
        if (_state == State::Connected)
        {
            // Reconnect to the same location again
            _connectingConfig = _connectedConfig;
            newState = State::Interrupted;
            queueConnectionAttempt();
        }
        break;

    case OpenVPNProcess::Exited:
        switch (_state)
        {
        case State::Connected:
            // Reconnect to the same location again
            _connectingConfig = _connectedConfig;
            newState = State::Interrupted;
            queueConnectionAttempt();
            break;
        case State::DisconnectingToReconnect:
            // If we were connected before, go to the 'reconnecting' state, not
            // the 'connecting' state.  If the location stayed the same, the
            // client will show a specific 'reconnecting' notification.
            if(_connectedConfig.vpnLocation())
                newState = State::Reconnecting;
            else
                newState = State::Connecting;
            scheduleNextConnectionAttempt();
            break;
        case State::Connecting:
        case State::StillConnecting:
        case State::Reconnecting:
        case State::StillReconnecting:
            scheduleNextConnectionAttempt();
            break;
        case State::Interrupted:
            newState = State::Reconnecting;
            scheduleNextConnectionAttempt();
            break;
        case State::Disconnecting:
            newState = State::Disconnected;
            break;
        default:
            qWarning() << "OpenVPN exited in unexpected state" << qEnumToString(_state);
            break;
        }
        break;

    default:
        break;
    }
    setState(newState);
}

void VPNConnection::openvpnExited(int exitCode)
{
    if (_networkAdapter)
    {
        // Ensure we return our tunnel metric to how it was before we lowered it.
        _networkAdapter->restoreOriginalMetric();
        _networkAdapter.clear();
    }
    _openvpn->deleteLater();
    _openvpn = nullptr;
    openvpnStateChanged();
}

void VPNConnection::openvpnError(const Error& err)
{
    raiseError(err);
}

void VPNConnection::raiseError(const Error& err)
{
    switch (err.code())
    {
    // Non-critical errors that are merely warnings
    //case ...:
    //  break;

    // Hard errors that should abort all connection attempts.
    // Few errors should really be fatal - only errors that definitely will not
    // resolve by themselves.  If there's any possibility that an error might
    // resolve with no user intervention, we should keep trying to connect.
    case Error::OpenVPNAuthenticationError:
        disconnectVPN();
        break;

    // This error should abort the current connection attempt, but we keep attempting to reconnect
    // as a user may fix their DNS enabling an eventual connection.
    case Error::OpenVPNDNSConfigError:
    // fallthrough

    // All other errors should cancel current attempt and retry
    default:
        if (_openvpn)
            _openvpn->shutdown();
        else
            queueConnectionAttempt();
    }
    emit error(err);
}

void VPNConnection::setState(State state)
{
    if (state != _state)
    {
        if(state == State::Disconnected)
        {
            // We have completely disconnected, drop the measurement intervals.
            _intervalMeasurements.clear();
            emit byteCountsChanged();

            // Stop shadowsocks if it was running.
            _shadowsocksRunner.disable();
        }

        // Several members are only valid in the [Still]Connecting and
        // [Still]Reconnecting states.  If we're going to any other state, clear
        // them.
        if(state != State::Connecting && state != State::StillConnecting &&
           state != State::Reconnecting && state != State::StillReconnecting)
        {
            _connectionStep = ConnectionStep::Initializing;
            _connectionAttemptCount = 0;
            _connectTimer.stop();
        }

        // In any state other than Connected, stop hnsd, even if that's our
        // current DNS setting.  (If we're reconnecting while Handshake is
        // selected, it'll be restarted after we connect.)
        if(state != State::Connected)
            _hnsdRunner.disable();

        _state = state;

        // Sanity-check location invariants and grab transports if they're
        // reported in this state
        nullable_t<Transport> preferredTransport, actualTransport;
        switch(_state)
        {
        default:
            Q_ASSERT(false);
            break;
        case State::Connecting:
        case State::StillConnecting:
            Q_ASSERT(_connectingConfig.vpnLocation());
            Q_ASSERT(!_connectedConfig.vpnLocation());
            preferredTransport = _transportSelector.preferred();
            break;
        case State::Reconnecting:
        case State::StillReconnecting:
            Q_ASSERT(_connectingConfig.vpnLocation());
            Q_ASSERT(_connectedConfig.vpnLocation());
            preferredTransport = _transportSelector.preferred();
            break;
        case State::Interrupted:
            Q_ASSERT(_connectingConfig.vpnLocation());
            Q_ASSERT(_connectedConfig.vpnLocation());
            break;
        case State::Connected:
            Q_ASSERT(!_connectingConfig.vpnLocation());
            Q_ASSERT(_connectedConfig.vpnLocation());
            preferredTransport = _transportSelector.preferred();
            actualTransport = _transportSelector.lastUsed();
            break;
        case State::Disconnecting:
        case State::Disconnected:
            Q_ASSERT(!_connectingConfig.vpnLocation());
            // _connectedConfig.vpnLocation() depends on whether we had a connection before
            break;
        case State::DisconnectingToReconnect:
            Q_ASSERT(_connectingConfig.vpnLocation());
            // _connectedConfig.vpnLocation() depends on whether we had a connection before
            break;
        }

        // Resolve '0' ports to actual effective ports
        if(_connectedConfig.vpnLocation())
        {
            if(preferredTransport)
                preferredTransport->resolvePort(*_connectedConfig.vpnLocation());
            if(actualTransport)
                actualTransport->resolvePort(*_connectedConfig.vpnLocation());
        }

        emit stateChanged(_state, _connectingConfig, _connectedConfig,
                          preferredTransport, actualTransport);
    }
}

void VPNConnection::updateByteCounts(quint64 received, quint64 sent)
{
    quint64 intervalReceived = received - _lastReceivedByteCount;
    quint64 intervalSent = sent - _lastSentByteCount;
    _lastReceivedByteCount = received;
    _lastSentByteCount = sent;

    // Add to perpetual totals
    _receivedByteCount += intervalReceived;
    _sentByteCount += intervalSent;

    // If we've reached the maximum number of measurements, discard one before
    // adding a new one
    if(_intervalMeasurements.size() == g_maxMeasurementIntervals)
        _intervalMeasurements.takeFirst();
    _intervalMeasurements.push_back({intervalReceived, intervalSent});

    // The interval measurements always change even if the perpetual totals do
    // not (we added a 0,0 entry).
    emit byteCountsChanged();
}

void VPNConnection::scheduleNextConnectionAttempt()
{
    quint64 remaining = _timeUntilNextConnectionAttempt.remainingTime();
    if (remaining > 0)
        _connectTimer.start((int)remaining);
    else
        queueConnectionAttempt();
}

void VPNConnection::queueConnectionAttempt()
{
    // Begin a new connection attempt now
    QMetaObject::invokeMethod(this, &VPNConnection::beginConnection, Qt::QueuedConnection);
}

bool VPNConnection::copySettings(State successState, State failureState)
{
    // successState must be a state where the connecting locations are valid
    Q_ASSERT(successState == State::Connecting || successState == State::StillConnecting ||
             successState == State::Reconnecting || successState == State::StillReconnecting ||
             successState == State::DisconnectingToReconnect ||
             successState == State::Interrupted);
    // failureState must be a state where the connecting locations are clear
    Q_ASSERT(failureState == State::Disconnected ||
             failureState == State::Connected ||
             failureState == State::Disconnecting);

    QJsonObject settings;
    for (auto name : g_connectionSettingNames)
    {
        settings.insert(name, g_settings.get(name));
    }
    QString username = g_account.openvpnUsername();
    QString password = g_account.openvpnPassword();
    DaemonSettings::DNSSetting dnsServers = g_settings.overrideDNS();
    ConnectionConfig newConfig{g_settings, g_state};

    bool changed = _connectionSettings != settings ||
        _openvpnUsername != username || _openvpnPassword != password ||
        dnsServers != _dnsServers || newConfig.hasChanged(_connectingConfig);

    _connectionSettings.swap(settings);
    _openvpnUsername.swap(username);
    _openvpnPassword.swap(password);
    _dnsServers = std::move(dnsServers);
    _connectingConfig = std::move(newConfig);

    _needsReconnect = false;

    // Reset to the first attempt; settings have changed
    if(changed)
        _connectionAttemptCount = 0;

    // If we failed to load any required data, fail
    if(!_connectingConfig.canConnect())
    {
        qWarning() << "Failed to load a required location - VPN location:"
            << !!_connectingConfig.vpnLocation() << "- Proxy type:"
            << traceEnum(_connectingConfig.proxyType())
            << "- Shadowsocks location:"
            << !!_connectingConfig.shadowsocksLocation();
        qWarning() << "Go to state" << traceEnum(failureState) << "instead of"
            << traceEnum(successState) << "due to failure to load locations";
        // Clear everything
        _connectingConfig = {};
        // Go to the failure state
        setState(failureState);
        return false;
    }

    // Locations were loaded
    setState(successState);
    return true;
}

bool VPNConnection::writeOpenVPNConfig(QFile& outFile)
{
    QTextStream out{&outFile};
    const char endl = '\n';

    auto sanitize = [](const QString& s) {
        QString result;
        result.reserve(s.size());
        for (auto& c : s)
        {
            switch (c.unicode())
            {
            case '\n':
            case '\t':
            case '\r':
            case '\v':
            case '"':
            case '\'':
                break;
            default:
                result.push_back(c);
                break;
            }
        }
        return result;
    };

    if (!_connectingConfig.vpnLocation())
        return false;

    out << "pia-signal-settings" << endl;
    out << "client" << endl;
    out << "dev tun" << endl;

    QString remoteServer;
    if (_transportSelector.lastUsed().protocol() == QStringLiteral("tcp"))
    {
        out << "proto tcp-client" << endl;
        remoteServer = sanitize(_connectingConfig.vpnLocation()->tcpHost());
    }
    else
    {
        out << "proto udp" << endl;
        out << "explicit-exit-notify" << endl;
        remoteServer = sanitize(_connectingConfig.vpnLocation()->udpHost());
    }
    if (remoteServer.isEmpty())
        return false;

    out << "remote " << remoteServer << ' ' << _transportSelector.lastUsed().port() << endl;

    if (!_connectingConfig.vpnLocation()->serial().isEmpty())
        out << "verify-x509-name " << sanitize(_connectingConfig.vpnLocation()->serial()) << " name" << endl;

    // OpenVPN's default setting is 'ping-restart 120'.  This means it takes up
    // to 2 minutes to notice loss of connection.  (On some OSes/systems it may
    // notice a change in local network connectivity more quickly, but this is
    // not reliable and cannot detect connection loss due to an upstream
    // change.)  This affects both TCP and UDP.
    //
    // 2 minutes is longer than most users seem to wait for the app to
    // reconnect, based on feedback and reports they usually give up and try to
    // reconnect manually before then.
    //
    // We've also seen examples where OpenVPN does not seem to actually exit
    // following connection loss, though we have not been able to reproduce
    // this.  It's possible that this has to do with the default 'ping-restart'
    // vs. 'ping-exit'.
    //
    // Enable ping-exit 25 to attempt to detect connection loss more quickly and
    // ensure OpenVPN exits on connection loss.
    out << "ping 5" << endl;
    out << "ping-exit 25" << endl;

    out << "persist-remote-ip" << endl;
    out << "resolv-retry 0" << endl;
    out << "route-delay 0" << endl;
    out << "reneg-sec 0" << endl;
    out << "server-poll-timeout 10s" << endl;
    out << "tls-client" << endl;
    out << "tls-exit" << endl;
    out << "remote-cert-tls server" << endl;
    out << "auth-user-pass" << endl;
    out << "pull-filter ignore \"auth-token\"" << endl;

    // Increasing sndbuf/rcvbuf can boost throughput, which is what most users
    // prioritize. For now, just copy the values used in the previous client.
    out << "sndbuf 262144" << endl;
    out << "rcvbuf 262144" << endl;

    if (_connectingConfig.defaultRoute())
    {
        out << "redirect-gateway def1 bypass-dhcp";
        if (!g_settings.blockIPv6())
            out << " ipv6";
        // When using a SOCKS proxy, handle the VPN endpoint route ourselves.
        if(_connectingConfig.proxyType() != ConnectionConfig::ProxyType::None)
            out << " local";
        out << endl;

        // When using a SOCKS proxy, if that proxy is on the Internet, we need
        // to route it back to the default gateway (like we do with the VPN
        // server).
        if(_connectingConfig.proxyType() != ConnectionConfig::ProxyType::None)
        {
            QHostAddress remoteHost;
            switch(_connectingConfig.proxyType())
            {
                default:
                case ConnectionConfig::ProxyType::None:
                    Q_ASSERT(false);
                    break;
                case ConnectionConfig::ProxyType::Custom:
                    remoteHost = _connectingConfig.socksHost();
                    break;
                case ConnectionConfig::ProxyType::Shadowsocks:
                {
                    QSharedPointer<ShadowsocksServer> _pSsServer;
                    if(_connectingConfig.shadowsocksLocation())
                        _pSsServer = _connectingConfig.shadowsocksLocation()->shadowsocks();
                    if(_pSsServer)
                        remoteHost = ConnectionConfig::parseIpv4Host(_pSsServer->host());
                    break;
                }
            }
            // If the remote host address couldn't be parsed, fail now.
            if(remoteHost.isNull())
                throw Error{HERE, Error::Code::OpenVPNProxyResolveError};

            // We do _not_ want this route if the SOCKS proxy is on LAN or
            // localhost.  (It actually mostly works, but on Windows with a LAN
            // proxy it does reroute the traffic through the gateway.)
            //
            // This may be incorrect with nested LANs - a SOCKS proxy on the outer
            // LAN is detected as LAN, but we actually do want this route since it's
            // not on _our_ LAN.  We don't fully support nested LANs right now, and
            // the only fix for that is to actually read the entire routing table.
            // Users can work around this by manually creating this route to the
            // SOCKS proxy.
            if(isIpv4Local(remoteHost))
            {
                qInfo() << "Not creating route for local proxy" << remoteHost;
            }
            else
            {
                qInfo() << "Creating gateway route for Internet proxy"
                    << remoteHost;

                // OpenVPN still creates a route to the remote endpoint when doing
                // this, which might seem unnecessary but is still desirable if the
                // proxy is running on localhost.  (Or even if the proxy is not
                // actually on localhost but ends up communicating over this host,
                // such as a proxy running in a VM on this host.)
                out << "route " << remoteHost.toString()
                    << " 255.255.255.255 net_gateway 0" << endl;
            }

            // We still want to route the VPN server back to the default gateway
            // too.  If the proxy communicates via localhost, we need this
            // route.  (Otherwise, traffic would be recursively routed back into
            // the proxy.)  This is needed for proxies on localhost, on a VM
            // running on this host, etc., and it doesn't hurt anything when
            // it's not needed.  The killswitch still blocks these connections
            // by default though, so this also requires turning killswitch off
            // or manually creating firewall rules to exempt the proxy.
            //
            // We do this ourselves because OpenVPN inconsistently substitutes
            // the SOCKS proxy address in the redirect-gateway route (it's not
            // clear why it does that occasionally but not always).
            out << "route " << remoteServer << " 255.255.255.255 net_gateway 0"
                << endl;
        }
    }
    else
    {
        QStringList dnsServers = getDNSServers(_dnsServers);

        QString specialPiaAddress = QStringLiteral("209.222.18.222");
        // Always route this special address through the VPN even when not using
        // PIA DNS; it's also used for MACE and port forwarding
        out << "route " << specialPiaAddress << " 255.255.255.255 vpn_gateway 0" << endl;
        // Route DNS servers into the VPN (DNS is always sent through the VPN)
        for(const auto &dnsServer : dnsServers)
        {
            if(dnsServer != specialPiaAddress)
                out << "route " << dnsServer << " 255.255.255.255 vpn_gateway 0" << endl;
        }

// Only create a default route if we're NOT on mac (since openvpn routes seem to be broken on mac)
#ifndef Q_OS_MAC
        // Add a default route with much a worse metric, so traffic can still
        // be routed on the tunnel opt-in by binding to the tunnel interface.
        // On Linux, metrics have been observed as high as 20600 on wireless
        // interfaces.  OpenVPN is still using `route` though, which interprets
        // the metric as 16-bit signed, so 32000 is about as high as we can go.

        // REMOVING this on macos/linux as it's very broken - the inverse operation ends up deleting the default route with the REAL IP
        // and when creating the route it needs to be a bound route on macos anyway AND for some weird reason it ends up being a /32 route
        // rather than a default route - a bug in openvpn? either way, we should probably just create this route ourselves
        out << "route 0.0.0.0 0.0.0.0 vpn_gateway 32000" << endl;
#endif

        // Ignore pushed settings to add default route
        out << "pull-filter ignore \"redirect-gateway \"" << endl;
    }

    // Set the local address only for alternate transports
    const QHostAddress &localAddress = _transportSelector.lastLocalAddress();
    if(!localAddress.isNull())
    {
        out << "local " << localAddress.toString() << endl;
        // We can't use nobind with a specific local address.  We can set lport
        // to 0 to let the network stack pick an ephemeral port though.
        out << "lport " << g_settings.localPort() << endl;
    }
    else if (g_settings.localPort() == 0)
        out << "nobind" << endl;
    else
        out << "lport " << g_settings.localPort() << endl;

    out << "cipher " << sanitize(g_settings.cipher()) << endl;
    if (!g_settings.cipher().endsWith("GCM"))
        out << "auth " << sanitize(g_settings.auth()) << endl;

    if (g_settings.mtu() > 0)
    {
        // TODO: For UDP it's also possible to use "fragment" to enable
        // internal datagram fragmentation, allowing us to deal with whatever
        // is sent into the tunnel. Unfortunately, this is a setting that
        // needs to be matched on the server side; maybe in the future we can
        // amend pia-signal-settings with it?

        out << "mssfix " << g_settings.mtu() << endl;
    }

    if(_connectingConfig.proxyType() != ConnectionConfig::ProxyType::None)
    {
        // If the host resolve step failed, _socksRouteAddress is not set, fail.
        if(_connectingConfig.socksHost().isNull())
            throw Error{HERE, Error::Code::OpenVPNProxyResolveError};

        uint port = 0;
        // A Shadowsocks local proxy uses an ephemeral port
        if(_connectingConfig.proxyType() == ConnectionConfig::ProxyType::Shadowsocks)
            port = _shadowsocksRunner.localPort();
        else
            port = _connectingConfig.customProxy().port();

        // Default to 1080 ourselves - OpenVPN doesn't like 0 here, and we can't
        // leave the port blank if we also need to specify "stdin" for auth.
        if(port == 0)
            port = 1080;
        out << "socks-proxy " << _connectingConfig.socksHost().toString() << " " << port;
        // If we have a username / password, ask OpenVPN to prompt over the
        // management interface.
        if(!_connectingConfig.customProxy().username().isEmpty() ||
           !_connectingConfig.customProxy().password().isEmpty())
        {
            out << " stdin";
        }
        out << endl;

        // The default timeout is very long (2 minutes), use a shorter timeout.
        // This applies to both the SOCKS and OpenVPN connections, we might
        // apply this even when not using a proxy in the future.
        out << "connect-timeout 30" << endl;
    }

    // Always ignore pushed DNS servers and use our own settings.
    out << "pull-filter ignore \"dhcp-option DNS \"" << endl;
    out << "pull-filter ignore \"dhcp-option DOMAIN local\"" << endl;

    out << "<ca>" << endl;
    for (const auto& line : g_data.getCertificateAuthority(g_settings.serverCertificate()))
        out << line << endl;
    out << "</ca>" << endl;

    return true;
}
