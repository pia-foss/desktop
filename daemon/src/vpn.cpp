// Copyright (c) 2021 Private Internet Access, Inc.
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
#line SOURCE_FILE("vpn.cpp")

#include "vpn.h"
#include "vpnmethod.h"
#include "daemon.h"
#include "exec.h"
#include "path.h"
#include "brand.h"
#include "openvpnmethod.h"
#include "wireguardmethod.h"
#include "configwriter.h"
#include "apinetwork.h"

#include <QFile>
#include <QTextStream>
#include <QTcpSocket>
#include <QTimer>
#include <QHostInfo>
#include <QRandomGenerator>
#include <QNetworkProxy>

// For use by findInterfaceIp on Mac/Linux
#if defined(Q_OS_UNIX)
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include "win/win.h"
#endif

// List of settings which require a reconnection.
// TODO - Eliminate this and just compare ConnectionConfigs to determine when
// a reconnect is needed.  This remains for now because a few settings aren't
// handled by ConnectionConfig yet (mainly transport, handled by
// TransportSelector.)
static std::initializer_list<const char*> g_connectionSettingNames = {
    "remotePortUDP",
    "remotePortTCP",
    "blockIPv6",
    "windowsIpMethod"
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
    const QStringList &hnsdFixedArgs{"-n", resolverLocalAddress() + ":1053",
                                     "-r", resolverLocalAddress() + ":53",
                                     "--seeds", hnsdSeed + "," + hnscanSeed};

    // Restart strategy for local resolver processes
    const RestartStrategy::Params resolverRestart{std::chrono::milliseconds(100), // Min restart delay
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

    // Maximum time between bytecount intervals, if the interval exceeds this
    // limit we abandon the connection.  This is intended to detect waking from
    // sleep; see updateByteCounts()
    const std::chrono::minutes bytecountAbandonLimit{4};

    // Length of the random suffix added to Dedicated IP region usernames; see
    // ConnectionConfig::ConnectionConfig()
    const int dipUsernameRandSuffixChars{8};
}

ResolverRunner::ResolverRunner(RestartStrategy::Params restartParams)
    : ProcessRunner{std::move(restartParams)}, _activeResolver{Resolver::Unbound}
{
    setObjectName(QStringLiteral("resolver"));

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
                if(_activeResolver == Resolver::Handshake && line.contains(QByteArrayLiteral(" new height: ")))
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
                // For handshake, start (or restart) the sync timer.
                if(_activeResolver == Resolver::Handshake)
                    _hnsdSyncTimer.start();
            });
    // 'succeeded' means that the process has been running for long enough that
    // ProcessRunner considers it successful.  It hasn't necessarily synced any
    // blocks yet; don't emit hnsdSyncFailure() at all.
    connect(this, &ProcessRunner::succeeded, this,
            [this]()
            {
                emit resolverSucceeded(_activeResolver);
            });
    connect(this, &ProcessRunner::failed, this,
            [this](std::chrono::milliseconds failureDuration)
            {
                emit resolverFailed(_activeResolver, failureDuration);
                // Stop the sync timer since hnsd isn't running.  The failure
                // signal covers this state, avoid spuriously emitting a sync
                // failure too (though these states _can_ overlap so we do still
                // tolerate this in the client UI).
                _hnsdSyncTimer.stop();
            });
}

void ResolverRunner::setupProcess(UidGidProcess &process)
{
#ifdef Q_OS_LINUX
     if(hasNetBindServiceCapability())
         // Drop root privileges ("nobody" is a low-priv account that should exist on all Linux systems)
        process.setUser("nobody");
    else
        qWarning() << getResolverExecutable() << "did not have cap_net_bind_service set; running resolver as root.";
#endif
#ifdef Q_OS_UNIX
    // Setting this group allows us to manage hnsd firewall rules
    process.setGroup(BRAND_CODE "hnsd");
#endif
}

bool ResolverRunner::enable(Resolver resolver, QStringList arguments)
{
    _activeResolver = resolver;
#ifdef Q_OS_MACOS
    Exec::cmd(QStringLiteral("ifconfig"), {"lo0", "alias", ::resolverLocalAddress(), "up"});
#endif
    // Invoke the original
    return ProcessRunner::enable(getResolverExecutable(), std::move(arguments));
}

void ResolverRunner::disable()
{
    // Invoke the original
    ProcessRunner::disable();

    // Not syncing or failing to sync since hnsd is no longer enabled.
    _hnsdSyncTimer.stop();
    emit hnsdSyncFailure(false);

#ifdef Q_OS_MACOS
    QString out = Exec::cmdWithOutput(QStringLiteral("ifconfig"), {QStringLiteral("lo0")});

    // Only try to remove the alias if it exists
    if(out.contains(::resolverLocalAddress().toLatin1()))
    {
        Exec::cmd(QStringLiteral("ifconfig"), {"lo0", "-alias", resolverLocalAddress()});
    }
#endif
}

const Path &ResolverRunner::getResolverExecutable() const
{
    if(_activeResolver == Resolver::Handshake)
        return Path::HnsdExecutable;
    return Path::UnboundExecutable;
}

// Check if Hnsd can bind to low ports without requiring root
bool ResolverRunner::hasNetBindServiceCapability()
{
#ifdef Q_OS_LINUX
    QString out = Exec::cmdWithOutput(QStringLiteral("getcap"), {getResolverExecutable()});
    if(out.isEmpty())
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
    debug.nospace() << QStringLiteral("Network(gatewayIp: %1, interfaceName: %2, ipAddress: %3, ipAddress6: %4, gatewayIp6: %5)")
        .arg(netScan.gatewayIp(), netScan.interfaceName(), netScan.ipAddress(), netScan.ipAddress6(), netScan.gatewayIp6());

    return debug;
}

TransportSelector::TransportSelector()
    : TransportSelector{preferredTransportTimeout}
{
}

TransportSelector::TransportSelector(const std::chrono::seconds &transportTimeout)
    : _selected{QStringLiteral("udp"), 0},
      _lastPreferred{QStringLiteral("udp"), 0},
      _lastUsed{QStringLiteral("udp"), 0}, _alternates{},
      _nextAlternate{0}, _startAlternates{-1}, _transportTimeout{transportTimeout},
      _serverIndex{0},
      _triedAllServers{false}
{
}

void TransportSelector::addAlternates(const QString &protocol,
                                      const DescendingPortSet &ports)
{
    Transport nextTransport{protocol, 0};

    // Add the implicit default port (although this may be the same as one of
    // the listed ports)
    if(_selected != nextTransport)
        _alternates.emplace_back(nextTransport);

    for(quint16 port : ports)
    {
        nextTransport.port(port);
        if(_selected != nextTransport)
            _alternates.emplace_back(nextTransport);
    }
}

void TransportSelector::reset(const QString &protocol, uint port,
                              bool useAlternates,
                              const DescendingPortSet &udpPorts,
                              const DescendingPortSet &tcpPorts)
{
    // If the specified port isn't actually a possible choice for the current
    // infrastructure and protocol at all, use "Default" instead.
    if(protocol == QStringLiteral("udp") && udpPorts.count(port) == 0)
        port = 0;
    else if(protocol == QStringLiteral("tcp") && tcpPorts.count(port) == 0)
        port = 0;
    _selected = {protocol, port};
    _alternates.clear();
    _nextAlternate = 0;
    _startAlternates.setRemainingTime(msec(_transportTimeout));
    _useAlternateNext = false;
    // Reset the local address; doesn't really matter since we redetect it for
    // each beginAttempt()
    _lastLocalAddress.clear();
    _serverIndex = 0;
    _triedAllServers = false;

    if(useAlternates)
    {
        // The expected count is udpPorts.size() + tcpPorts.size() + 2 - there
        // are two "default" choices in addition to the listed UDP and TCP
        // ports.
        _alternates.reserve(udpPorts.size() + tcpPorts.size() + 2);

        // Prefer to stay on the user's selected protocol; try those first.
        if(_selected.protocol() == QStringLiteral("udp"))
        {
            addAlternates(QStringLiteral("udp"), udpPorts);
            addAlternates(QStringLiteral("tcp"), tcpPorts);
        }
        else
        {
            addAlternates(QStringLiteral("tcp"), tcpPorts);
            addAlternates(QStringLiteral("udp"), udpPorts);
        }
    }
}

QHostAddress TransportSelector::lastLocalAddress() const
{
    // If the last transport is the preferred transport, always allow any local
    // address, even if we have alternate transport configurations to try.
    //
    // This ensures that we behave the same as prior releases most of the time,
    // either when "Try Alternate Transports" is turned off or for connections
    // using the preferred transport setting when it is on.
    if(_lastUsed == _lastPreferred)
        return {};

    // When using an alternate transport, restrict to the last local address we
    // found.  If the network connection changes, we don't want an alternate
    // transport to succeed by chance, we want it to fail so we can try the
    // preferred transport again.
    return _lastLocalAddress;
}

const Server *TransportSelector::beginAttempt(const Location &location,
                                              const QHostAddress &localAddress,
                                              bool &delayNext)
{
    // If the address has changed, a network connectivity change has
    // occurred.  Reset and try the preferred transport only for a while.
    //
    // If Try Alternate Transports is turned off, this has no effect, because we
    // don't have any alternate transports to try in that case.
    if(localAddress != _lastLocalAddress)
    {
        qInfo() << "Would use:" << localAddress << "to reach default gateway";
        qInfo() << "Network connectivity has changed since last attempt, start over from preferred transport";
        _lastLocalAddress = localAddress;
        _nextAlternate = 0;
        _startAlternates.setRemainingTime(msec(_transportTimeout));
        _useAlternateNext = false;
    }

    delayNext = true;

    const Server *pSelectedServer{nullptr};

    // The number of servers for this location (constrained by port and protocol)
    const std::size_t serverCount{_selected.countServersForLocation(location)};

    // No servers available
    if(serverCount == 0)
    {
        qWarning() << "No servers are available for the selected OpenVPN transport";
        return nullptr;
    }

    // Always use the preferred transport if:
    // - We haven't yet tried all servers (if there's > 1 server)
    // - there are no alternates
    // - the preferred transport interval hasn't elapsed
    // - we failed to detect a local IP address for the connection (this means
    //   we are not connected to a network right now, and we don't want to
    //   attempt an alternate transport with "any" local address)
    if((!_triedAllServers && serverCount > 1) || _alternates.empty() || !_startAlternates.hasExpired() ||
       _lastLocalAddress.isNull())
    {
        _lastUsed = _selected;
        // Attempt to connect to the next server for this location
        pSelectedServer = _lastUsed.selectServerPortWithIndex(location, _serverIndex);

        if(_serverIndex + 1 == serverCount)
            // We've tried every server for this location
            _triedAllServers = true;

        // Only turn off connection delays if we haven't yet
        // tried every server. Otherwise successive connection
        // attempts will be delayed.
        if(!_triedAllServers)
            delayNext = false;

        _serverIndex = (_serverIndex + 1) % serverCount;
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
        _lastUsed = _selected;
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

    _lastPreferred = _selected;

    if(!pSelectedServer)
        pSelectedServer = _lastUsed.selectServerPort(location);

    _lastPreferred.resolveDefaultPort(_lastUsed.protocol(), pSelectedServer);
    return pSelectedServer;
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
            return {};
        }
    }
    else
    {
        qInfo() << "Invalid SOCKS proxy address:" << host;
    }

    return socksTestAddress;
}

ConnectionConfig::ConnectionConfig()
{}

ConnectionConfig::ConnectionConfig(DaemonSettings &settings, DaemonState &state,
                                   DaemonAccount &account)
    : ConnectionConfig{}
{
    // Grab the next VPN location.  Copy it in case the locations in DaemonState
    // are updated.
    if(state.vpnLocations().nextLocation())
        _pVpnLocation.reset(new Location{*state.vpnLocations().nextLocation()});
    _vpnLocationAuto = !state.vpnLocations().chosenLocation();

    const auto &methodValue = settings.method();
    if(methodValue == QStringLiteral("openvpn"))
        _method = Method::OpenVPN;
    else if(methodValue == QStringLiteral("wireguard"))
        _method = Method::Wireguard;
    // Any other value is treated as "openvpn"; the setting values are validated
    // by the daemon so invalid values should not occur.
    else
    {
        _method = Method::OpenVPN;
        qWarning() << "Unexpected method setting" << methodValue
            << "- using OpenVPN";
    }

    // Get the credentials that will be used to authenticate
    // For dedicated IP regions, use the DIP token
    if(_pVpnLocation && _pVpnLocation->isDedicatedIp())
    {
        // Dedicated IP region, use DIP token credentials.  Find the DIP token
        // from the account information
        const auto &accountDips = account.dedicatedIps();
        auto itAccountDip = std::find_if(accountDips.begin(), accountDips.end(),
            [this](const AccountDedicatedIp &accountDip)
            {
                return accountDip.id() == _pVpnLocation->id();
            });
        if(itAccountDip != accountDips.end())
        {
            // The random identifier at the end of the username just allows the
            // server to accept multiple simultaneous connections on the same
            // dedicated IP, as some auth backends can't handle more than one
            // connection using the same username.
            _vpnUsername = QStringLiteral("dedicated_ip_") +
                itAccountDip->dipToken() +
                QStringLiteral("_%1").arg(QRandomGenerator::global()->generate(),
                               dipUsernameRandSuffixChars, 16, QChar{'0'});
            _vpnPassword = itAccountDip->ip();
        }
    }
    else
    {
        // Normal region, use account authentication (credentials or token)
        _vpnUsername = account.openvpnUsername();
        _vpnPassword = account.openvpnPassword();
        _vpnToken = account.token();

        // WireGuard auth requires a token.  If we haven't been able to obtain a
        // token, use OpenVPN.
        if(_method == Method::Wireguard && _vpnToken.isEmpty())
        {
            qInfo() << "Using OpenVPN instead of WireGuard for this connection, auth token is not available";
            _method = Method::OpenVPN;
            _methodForcedByAuth = true;
        }
    }

    // Read the DNS setting
    const auto &dnsSetting = settings.overrideDNS();
    if(dnsSetting == QStringLiteral("pia"))
        _dnsType = DnsType::Pia;
    else if(dnsSetting == QStringLiteral("handshake"))
        _dnsType = DnsType::Handshake;
    else if(dnsSetting == QStringLiteral("local"))
        _dnsType = DnsType::Local;
    // Otherwise, check if it's a QStringList of server addresses
    else if(dnsSetting.get(_customDns) && !_customDns.isEmpty())
        _dnsType = DnsType::Custom;
    else
    {
        _dnsType = DnsType::Existing;
        // Don't set the default DNS, since we're using existing DNS.
        _setDefaultDns = false;
    }

    _localPort = static_cast<quint16>(settings.localPort());
    _mtu = settings.mtu();

    _requestMace = settings.enableMACE();

    if(settings.splitTunnelEnabled())
    {
        _otherAppsUseVpn = settings.defaultRoute();

        // On Windows and Linux, _setDefaultRoute follows _otherAppsUseVPN.
        // On Mac, _setDefaultRoute is always false when split tunnel is enabled
        // due to the split tunnel device taking the default route.
#if defined(Q_OS_MAC)
        _setDefaultRoute = false;
#else
        _setDefaultRoute = _otherAppsUseVpn;
#endif

        // Do we want to split DNS too?
        // (Not currently available on Mac)
#if !defined(Q_OS_MAC)
        if(_dnsType != DnsType::Existing && settings.splitTunnelDNS())
        {
            // Yes - set the default DNS only if the VPN has the default route
            _setDefaultDns = _setDefaultRoute;
            // Force VPN-only apps to use PIA's configured DNS if the VPN does
            // not have the default route
            _forceVpnOnlyDns = !_setDefaultRoute;
            // Force bypass apps to use existing DNS if the VPN has the default
            // route
            _forceBypassDns = _setDefaultRoute;
        }
#endif
        // Otherwise, not splitting DNS - either split tunnel DNS is disabled,
        // or we're using existing DNS anyway.
    }

    // Capture OpenVPN-specific settings.
    if(_method == Method::OpenVPN)
    {
        _openvpnCipher = settings.cipher();
        if(settings.protocol() == QStringLiteral("tcp"))
            _openvpnProtocol = Protocol::TCP;

        // Proxy and automatic transport require OpenVPN.
        if(settings.proxyEnabled() && settings.proxyType() == QStringLiteral("custom"))
        {
            _proxyType = ProxyType::Custom;
            _customProxy = settings.proxyCustom();

            // The proxy address must be a literal IPv4 address, we cannot resolve
            // hostnames due DNS being blocked during reconnection.  For any
            // failure, leave _socksHostAddress clear, which will be detected as a
            // nonfatal error later.
            _socksHostAddress = parseIpv4Host(_customProxy.host());
        }
        else if(settings.proxyEnabled() && settings.proxyType() == QStringLiteral("shadowsocks"))
        {
            _proxyType = ProxyType::Shadowsocks;
            // PIA's Shadowsocks servers don't have UDP enabled, force TCP.
            _openvpnProtocol = Protocol::TCP;
            _socksHostAddress = QHostAddress{0x7f000001}; // 127.0.0.1
            if(state.shadowsocksLocations().nextLocation())
                _pShadowsocksLocation.reset(new Location{*state.shadowsocksLocations().nextLocation()});
            _shadowsocksLocationAuto = !state.shadowsocksLocations().chosenLocation();
        }
        else
        {
            _proxyType = ProxyType::None;
        }
        
        // The protocol might have been forced to TCP if Shadowsocks was enabled.
        if(_openvpnProtocol == Protocol::TCP)
            _openvpnRemotePort = settings.remotePortTCP();
        else
            _openvpnRemotePort = settings.remotePortUDP();

        // Automatic transport - can't be used with a proxy (we could not be
        // confident that a failure to connect is due to a port being
        // unreachable)
        if(_proxyType == ProxyType::None)
            _automaticTransport = settings.automaticTransport();
    }
    // Capture WireGuard-specific settings
    else if(_method == Method::Wireguard)
        _wireguardUseKernel = settings.wireguardUseKernel();

    // The port forwarding setting is more complex, because changes are
    // applied on the fly in some cases, but require reconnects in others.
    //
    // When connected to a region that supports PF using a connection that
    // supports PF, any change requires a reconnect.  We have to reconnect
    // to start a new PF request, or when disabling PF, we keep the port
    // that we got until we reconnect.
    //
    // When connected to a region that doesn't support it, changing the
    // setting just toggles between Inactive and Unavailable.  This doesn't
    // require a reconnect.
    //
    // As a result, the connection config only captures the PF setting when
    // connecting to a region that has PF.  Otherwise, the connection config
    // does not depend on PF (changes will not trigger a reconnect notice),
    // and the daemon allows PortForwarder to track the real setting as it's
    // updated to toggle between Inactive/Unavailable.
    if(_pVpnLocation && _pVpnLocation->portForward())
        _requestPortForward = settings.portForward();
}

bool ConnectionConfig::canConnect() const
{
    if(!vpnLocation())
    {
        qWarning() << "No VPN location found, cannot connect";
        return false;   // Always required for any connection
    }

    // If we weren't able to get any credentials, we can't connect.
    // This shouldn't happen, but if something goes wrong with DIP regions, it
    // might occur.
    if(vpnUsername().isEmpty() && vpnToken().isEmpty())
    {
        qWarning() << "No VPN credentials found, cannot connect";
        return false;
    }

    switch(proxyType())
    {
        default:
        case ProxyType::None:
            break;
        case ProxyType::Shadowsocks:
            if(!shadowsocksLocation() || !shadowsocksLocation()->hasService(Service::Shadowsocks))
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

    // For dedicated IP locations, the "username" used to authenticate has
    // random data on the end of it since some auth backends can't handle more
    // than one simultaneous connection with the same username.  Ignore this; it
    // doesn't indicate settings have changed.
    QString userFixedPart{vpnUsername()};
    if(userFixedPart.size() >= dipUsernameRandSuffixChars && vpnLocation() &&
        vpnLocation()->isDedicatedIp())
    {
        userFixedPart = userFixedPart.left(userFixedPart.size() - dipUsernameRandSuffixChars);
    }
    QString otherUserFixedPart{vpnUsername()};
    if(otherUserFixedPart.size() >= dipUsernameRandSuffixChars && other.vpnLocation() &&
        other.vpnLocation()->isDedicatedIp())
    {
        otherUserFixedPart = otherUserFixedPart.left(otherUserFixedPart.size() - dipUsernameRandSuffixChars);
    }

    return method() != other.method() ||
        methodForcedByAuth() != other.methodForcedByAuth() ||
        userFixedPart != otherUserFixedPart ||
        vpnPassword() != other.vpnPassword() ||
        vpnToken() != other.vpnToken() ||
        openvpnCipher() != other.openvpnCipher() ||
        openvpnProtocol() != other.openvpnProtocol() ||
        openvpnRemotePort() != other.openvpnRemotePort() ||
        wireguardUseKernel() != other.wireguardUseKernel() ||
        localPort() != other.localPort() ||
        mtu() != other.mtu() ||
        automaticTransport() != other.automaticTransport() ||
        dnsType() != other.dnsType() ||
        customDns() != other.customDns() ||
        requestMace() != other.requestMace() ||
        setDefaultDns() != other.setDefaultDns() ||
        forceVpnOnlyDns() != other.forceVpnOnlyDns() ||
        forceBypassDns() != other.forceBypassDns() ||
        otherAppsUseVpn() != other.otherAppsUseVpn() ||
        setDefaultRoute() != other.setDefaultRoute() ||
        proxyType() != other.proxyType() ||
        socksHost() != other.socksHost() ||
        customProxy() != other.customProxy() ||
        requestPortForward() != other.requestPortForward() ||
        vpnLocationId != otherVpnLocationId ||
        vpnLocationAuto() != other.vpnLocationAuto() ||
        ssLocationId != otherSsLocationId;
}

QStringList ConnectionConfig::getDnsServers() const
{
    switch(dnsType())
    {
        default:
        case DnsType::Pia:
            return {requestMace() ? piaModernDnsVpnMace() : piaModernDnsVpn()};
        case DnsType::Handshake:
        case DnsType::Local:
            return {resolverLocalAddress()};
        case DnsType::Existing:
            return {};
        case DnsType::Custom:
            return customDns();
    }
}

VPNConnection::VPNConnection(QObject* parent)
    : QObject(parent)
    , _state(State::Disconnected)
    , _connectionStep{ConnectionStep::Initializing}
    , _method(nullptr)
    , _resolverRunner{resolverRestart}
    , _shadowsocksRunner{shadowsocksRestart}
    , _connectionAttemptCount(0)
    , _receivedByteCount(0)
    , _sentByteCount(0)
    , _lastReceivedByteCount(0)
    , _lastSentByteCount(0)
    , _lastBytecountTime{}
    , _needsReconnect(false)
{
    _shadowsocksRunner.setObjectName("shadowsocks");

    _connectTimer.setSingleShot(true);
    connect(&_connectTimer, &QTimer::timeout, this, &VPNConnection::beginConnection);

    connect(&_resolverRunner, &ResolverRunner::resolverSucceeded, this,
        [this](ResolverRunner::Resolver _resolver)
        {
            if(_resolver == ResolverRunner::Resolver::Handshake)
                emit hnsdSucceeded();
            else
                emit unboundSucceeded();
        });
    connect(&_resolverRunner, &ResolverRunner::resolverFailed, this,
        [this](ResolverRunner::Resolver _resolver, std::chrono::milliseconds failureDuration)
        {
            if(_resolver == ResolverRunner::Resolver::Handshake)
                emit hnsdFailed(failureDuration);
            else
                emit unboundFailed(failureDuration);
        });
    connect(&_resolverRunner, &ResolverRunner::hnsdSyncFailure, this, &VPNConnection::hnsdSyncFailure);

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
        if((_state == State::Connecting || _state == State::Reconnecting) &&
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

    // Clean up all supported VPN methods (that have cleanup), to ensure nothing
    // is left over in case the daemon had crashed
    cleanupWireguard();
}

void VPNConnection::scheduleDnsCacheFlush()
{
    qInfo() << "Scheduling DNS cache flush";
    QTimer::singleShot(1000, this, [this]()
    {
        qInfo() << "Flushing DNS cache";
#if defined(Q_OS_LINUX)
        Exec::bash(QStringLiteral("if [[ $(realpath /etc/resolv.conf) =~ systemd ]]; then systemd-resolve --flush-caches; fi"));
#elif defined(Q_OS_MAC)
        Exec::cmd(QStringLiteral("dscacheutil"), {QStringLiteral("-flushcache")});
        Exec::cmd(QStringLiteral("discoveryutil"), {QStringLiteral("udnsflushcaches")});
        Exec::cmd(QStringLiteral("discoveryutil"), {QStringLiteral("mdnsflushcache")});
        Exec::cmd(QStringLiteral("killall"), {QStringLiteral("-HUP"), QStringLiteral("mDNSResponder")});
        Exec::cmd(QStringLiteral("killall"), {QStringLiteral("-HUP"), QStringLiteral("mDNSResponderHelper")});
#elif defined(Q_OS_WIN)
        Exec::cmd(QStringLiteral("ipconfig"), {QStringLiteral("/flushdns")});
#endif
    });
}

bool VPNConnection::needsReconnect()
{
    if (!_method || _state == State::Disconnecting || _state == State::DisconnectingToReconnect || _state == State::Disconnected)
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
    ConnectionConfig newConfig{g_settings, g_state, g_account};
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
    case State::Reconnecting:
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

void VPNConnection::updateNetwork(const OriginalNetworkScan &newNetwork)
{
    if(_method)
        _method->updateNetwork(newNetwork);
}

bool VPNConnection::connectVPN(bool force)
{
    switch (_state)
    {
    case State::Connected:
        Q_ASSERT(_connectedConfig.vpnLocation()); // Valid in this state
        // If settings haven't changed, there's nothing to do.
        if (!force && !needsReconnect())
            return false;

        // Otherwise, change to DisconnectingToReconnect
        copySettings(State::DisconnectingToReconnect, State::Disconnecting);

        Q_ASSERT(_method); // Valid in this state
        _method->shutdown();
        return true;
    case State::Connecting:
    case State::Reconnecting:
        // If settings haven't changed, there's nothing to do.
        if (!force && !needsReconnect())
            return false;   // Still in same connection attempt
        // fallthrough
        if (_method)
        {
            _method->shutdown();
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
        return true;
    case State::DisconnectingToReconnect:
    case State::Interrupted:
        // In these states, we're already waiting for the VPN method to shut
        // down in order to start a new connection attempt.  There's nothing to
        // do.  We'll reconnect as planned with the new settings when the method
        // shuts down.
        //
        // The return value is still 'true' in this case if force was set, just
        // as if we had been in the Connecting state, because a reconnect was
        // still requested and we're doing it - it just happened that we didn't
        // actually have to initiate a reconnect due to timing.
        return force || needsReconnect();
    default:
        qWarning() << "Connecting in unhandled state " << _state;
        // fallthrough
    case State::Disconnected:
        updateAttemptCount(0);
        _connectedConfig = {};
        _connectedServer = {};
        if(copySettings(State::Connecting, State::Disconnected))
            queueConnectionAttempt();
        return true;
    }
}

void VPNConnection::disconnectVPN()
{
    if (_state != State::Disconnected)
    {
        _connectingConfig = {};
        _connectingServer = {};
        setState(State::Disconnecting);
        if (_method && _method->state() < VPNMethod::State::Exiting)
            _method->shutdown();
        if (!_method || _method->state() == VPNMethod::State::Exited)
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

bool VPNConnection::useSlowInterval() const
{
    return _connectionAttemptCount > Limits::SlowConnectionAttemptLimit;
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

    if (_method)
    {
        qWarning() << "VPN method already exists; doConnect ignored";
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
        if(_connectionAttemptCount == 0 && _state == State::Connecting)
        {
            // We only get one shot at this, clear the connection cache to make
            // sure we're not reusing an old bogus connection.  We're about to
            // connect anyway so it's fine to kill off any in-flight requests at
            // this point.
            //
            // The proxy-username hack in ApiNetwork doesn't apply when we're
            // not connected, since we don't use a proxy, so if the network
            // changes it might otherwise take ~2 minutes for stale connections
            // to die.
            ApiNetwork::instance()->getAccessManager().clearConnectionCache();
            // We're not retrying this request if it fails - we don't want to hold
            // up the connection attempt; this information isn't critical.
            g_daemon->apiClient().getIp(*g_daemon->environment().getIpAddrApi(), QStringLiteral("api/client/status"))
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

        // Select a Shadowsocks server and parse its IP address.  If Shadowsocks
        // isn't selected, ConnectionConfig does not capture a Shadowsocks
        // location (but this could also happen if Shadowsocks was selected and
        // no locations are known).
        _shadowsocksServerIp = {};
        const Server *pSsServer{nullptr};
        if(_connectingConfig.shadowsocksLocation())
            pSsServer = _connectingConfig.shadowsocksLocation()->randomServerForService(Service::Shadowsocks);
        // randomServerForService() ensures that a returned server has the
        // Shadowsocks service and at least one port, but it does not verify
        // that we have an SS key and cipher
        if(pSsServer && !pSsServer->shadowsocksKey().isEmpty() && !pSsServer->shadowsocksCipher().isEmpty())
            _shadowsocksServerIp = QHostAddress{pSsServer->ip()};

        // Was Shadowsocks actually selected for a proxy?
        if(_connectingConfig.proxyType() == ConnectionConfig::ProxyType::Shadowsocks)
        {
            // If we are not able to connect with Shadowsocks, raise an error
            // and bail, user asked for Shadowsocks.
            if(_shadowsocksServerIp.protocol() != QAbstractSocket::NetworkLayerProtocol::IPv4Protocol)
            {
                qWarning() << "Unable to connect - Shadowsocks was requested, but no server address is available in location"
                    << (_connectingConfig.shadowsocksLocation() ? _connectingConfig.shadowsocksLocation()->id() : QStringLiteral("<none>"))
                    << "- server:" << (pSsServer ? pSsServer->ip() : QStringLiteral("<none>"))
                    << "- key:" << (pSsServer ? pSsServer->shadowsocksKey() : QStringLiteral("<none>"))
                    << "- cipher:" << (pSsServer ? pSsServer->shadowsocksCipher() : QStringLiteral("<none>"));
                raiseError({HERE, Error::Code::VPNConfigInvalid});
                return;
            }

            _shadowsocksRunner.enable(Path::SsLocalExecutable,
                QStringList{QStringLiteral("-s"), pSsServer->ip(),
                            QStringLiteral("-p"), QString::number(pSsServer->defaultServicePort(Service::Shadowsocks)),
                            QStringLiteral("-k"), pSsServer->shadowsocksKey(),
                            QStringLiteral("-b"), QStringLiteral("127.0.0.1"),
                            QStringLiteral("-l"), QStringLiteral("0"),
                            QStringLiteral("-m"), pSsServer->shadowsocksCipher()});

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
        else    // Not using Shadowsocks
            _shadowsocksRunner.disable();
    }

    // We either finished starting a proxy or we skipped it.  We're ready to connect
    Q_ASSERT(_connectionStep == ConnectionStep::StartingProxy);
    _connectionStep = ConnectionStep::ConnectingOpenVPN;

    if (_connectionAttemptCount == 0)
    {
        Q_ASSERT(_connectingConfig.vpnLocation());  // Postcondition of copySettings() above

        // Reset the transport selection sequence.
        //
        // Try all ports that are available on any server in this region.  The
        // available ports might vary by server.
        //
        // If the ports do vary, then when we try a port that's not available
        // everywhere, we will specifically look for a server that supports that
        // port (by using Location::randomServer() with a port).
        //
        // It's also possible that the user selected a port that is not
        // available anywhere in this region.  In that case, we will use the
        // default port instead (by using Transport::resolveActualPort()), and
        // the UI will indicate that we used a different transport (since the
        // preferred transport still indicates the user's selection, due to
        // Transport::resolveDefaultPort()).
        QString protocolName{QStringLiteral("udp")};
        if(_connectingConfig.openvpnProtocol() == ConnectionConfig::Protocol::TCP)
            protocolName = QStringLiteral("tcp");
        _transportSelector.reset(protocolName, _connectingConfig.openvpnRemotePort(),
                                 _connectingConfig.automaticTransport(),
                                 _connectingConfig.vpnLocation()->allPortsForService(Service::OpenVpnUdp),
                                 _connectingConfig.vpnLocation()->allPortsForService(Service::OpenVpnTcp));
    }

    // Reset traffic counters since we have a new process
    _lastReceivedByteCount = 0;
    _lastSentByteCount = 0;
    _intervalMeasurements.clear();
    emit byteCountsChanged();

    // Reset any running connect timer, just in case
    _connectTimer.stop();

    Q_ASSERT(_connectingConfig.vpnLocation());  // Postcondition of copySettings()
    OriginalNetworkScan netScan = g_daemon->originalNetwork();
    qInfo() << "Initial netScan for VPN method" << netScan;

    bool delayNext = true;
    const Server *pVpnServer = _transportSelector.beginAttempt(*_connectingConfig.vpnLocation(), QHostAddress{netScan.ipAddress()}, delayNext);
    // When using WireGuard, the TransportSelector is vestigial, since
    // WireGuard currently only supports one protocol and port.  It's still
    // active right now so transports are reported, but it reports the selected
    // OpenVPN transport (which is OK for the moment, it prevents any "alternate
    // transport" notifications).  We still need to really find a WireGuard
    // server.
    if(_connectingConfig.method() == ConnectionConfig::Method::Wireguard)
    {
        const auto location = _connectingConfig.vpnLocation();
        const auto serverCount{location->countServersForService(Service::WireGuard)};

        if(serverCount != 0)
        {
            // Attempt to connect to the next server for this location
            pVpnServer = location->serverWithIndexForService(_connectionAttemptCount % serverCount, Service::WireGuard);
        }
        else
        {
            // No servers available
            qWarning() << "No WireGuard servers are available in this location";
            pVpnServer = nullptr;
        }
    }

    // Set when the next earliest reconnect attempt is allowed
    if(delayNext)
    {
        if(useSlowInterval())
            _timeUntilNextConnectionAttempt.setRemainingTime(Limits::SlowConnectionAttemptInterval);
        else
            _timeUntilNextConnectionAttempt.setRemainingTime(Limits::ConnectionAttemptInterval);
    }
    else
        _timeUntilNextConnectionAttempt.setRemainingTime(0);

    updateAttemptCount(_connectionAttemptCount+1);

    if(!pVpnServer)
    {
        qWarning() << "Could not find a server in location"
            << _connectingConfig.vpnLocation()->id() << "for method"
            << traceEnum(_connectingConfig.method()) << "and transport"
            << _transportSelector.lastUsed().protocol();
        if(_connectionAttemptCount % 100 == 1)
        {
            qWarning() << "Cached modern regions list:"
                << QJsonDocument{g_data.cachedModernRegionsList()}.toJson();
        }
        _connectingServer = {};
        scheduleNextConnectionAttempt();
        return;
    }

    _connectingServer = *pVpnServer;

    switch(_connectingConfig.method())
    {
        case ConnectionConfig::Method::OpenVPN:
            _method = new OpenVPNMethod{this, netScan};
            break;
        case ConnectionConfig::Method::Wireguard:
            _method = createWireguardMethod(this, netScan).release();
            break;
        default:
            Q_ASSERT(false);
            break;
    }
    connect(_method, &VPNMethod::stateChanged, this, &VPNConnection::vpnMethodStateChanged);
    connect(_method, &VPNMethod::tunnelConfiguration, this,
            &VPNConnection::usingTunnelConfiguration);
    connect(_method, &VPNMethod::bytecount, this, &VPNConnection::updateByteCounts);
    connect(_method, &VPNMethod::firewallParamsChanged, this, &VPNConnection::firewallParamsChanged);
    connect(_method, &VPNMethod::error, this, &VPNConnection::raiseError);

    QHostAddress localBindAddress = _transportSelector.lastLocalAddress();

#if defined(Q_OS_MAC)
    // On Mac, if split tunnel is enabled, the split tunnel device gets the
    // default route, even when not connected.  We need OpenVPN to bind to the
    // physical interface whenever we're not going to set the default route to
    // bypass this - even if we're using the preferred transport.
    if(!_connectingConfig.setDefaultRoute())
        localBindAddress = QHostAddress{netScan.ipAddress()};
#endif

    try
    {
        _method->run(_connectingConfig, _connectingServer,
                     _transportSelector.lastUsed(), localBindAddress,
                     _shadowsocksServerIp, _shadowsocksRunner.localPort());
    }
    catch(const Error &ex)
    {
        raiseError(ex);
    }
}

void VPNConnection::vpnMethodStateChanged()
{
    VPNMethod::State methodState = _method ? _method->state() : VPNMethod::State::Exited;
    State newState = _state;

    switch (methodState)
    {
    case VPNMethod::State::Connected:
        switch (_state)
        {
        default:
            qWarning() << "VPN method connected in unexpected state" << qEnumToString(_state);
            Q_FALLTHROUGH();
        case State::Connecting:
        case State::Reconnecting:
            // The connection was established, so the connecting location is now
            // the connected location.
            _connectedConfig = std::move(_connectingConfig);
            _connectedServer = std::move(_connectingServer);
            _connectingConfig = {};
            _connectingServer = {};

            // If DNS is set to Handshake, start it now, since we've connected
            if(_connectedConfig.dnsType() == ConnectionConfig::DnsType::Handshake)
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
                _resolverRunner.enable(ResolverRunner::Resolver::Handshake, hnsdArgs);
            }
            else if(_connectedConfig.dnsType() == ConnectionConfig::DnsType::Local)
            {
                // Write the config file
                {
                    ConfigWriter conf{Path::UnboundConfigFile};
                    conf << "server:" << conf.endl;
                    conf << "    logfile: \"\"" << conf.endl;   // Log to stderr
                    conf << "    edns-buffer-size: 4096" << conf.endl;
                    conf << "    max-udp-size: 4096" << conf.endl;
                    conf << "    qname-minimisation: yes" << conf.endl;
                    conf << "    do-ip6: no" << conf.endl;
                    conf << "    interface: " << resolverLocalAddress() << conf.endl;
                    conf << "    outgoing-interface:" << g_state.tunnelDeviceLocalAddress() << conf.endl;
                    conf << "    verbosity: 1" << conf.endl;
                    // We can't let unbound drop rights, even on Mac/Linux - it
                    // drops both user and group rights, and we need it to keep
                    // the piavpn group to be permitted through the firewall.
                    //
                    // On Linux, if the cap_net_bind_service capability is
                    // available, ResolverRunner will drop to nobody/piavpn.
                    conf << "    username: \"\"" << conf.endl;
                    conf << "    do-daemonize: no" << conf.endl;
                    conf << "    use-syslog: no" << conf.endl;
                    conf << "    hide-identity: yes" << conf.endl;
                    conf << "    hide-version: yes" << conf.endl;
                    conf << "    directory: \"" << Path::InstallationDir << "\"" << conf.endl;
                    conf << "    pidfile: \"\"" << conf.endl;
                    conf << "    chroot: \"\"" << conf.endl;
                }
                _resolverRunner.enable(ResolverRunner::Resolver::Unbound, {"-c", Path::UnboundConfigFile});
            }

            // For any DNS method other than "Use Existing DNS", schedule a
            // DNS cache flush.
            //
            // Changing the DNS configuration typically causes the system to
            // flush DNS caches on its own (and the DNS helper scripts also
            // trigger it when needed), but we've observed that on some systems
            // it's still possible to get non-VPN DNS entries cached after the
            // connection is established (notably with systemd-resolv on
            // Ubuntu 20.04.1).
            //
            // This can't be reproduced reliabily and likely involves races
            // between in-flight DNS requests and the DNS configuration change.
            // Scheduling a deferred DNS cache wipe improves this.  (This is
            // mostly noticeable with MACE, but DNS resolution can differ in
            // other circumstances too, so we do this even if MACE is not
            // enabled.)
            if(_connectedConfig.dnsType() != ConnectionConfig::DnsType::Existing)
                scheduleDnsCacheFlush();

            newState = State::Connected;
            break;
        case State::Disconnecting:
        case State::Disconnected:
        case State::DisconnectingToReconnect:
            // In these cases, we should have already told it to shutdown, and
            // the connection raced with the shutdown.  Just in case, tell it to
            // shutdown again; can't hurt anything and ensures that we don't get
            // stuck in this state.
            if (_method)
                _method->shutdown();
            break;
        }
        break;
    case VPNMethod::State::Exiting:
        if (_state == State::Connected)
        {
            // Reconnect to the same location again
            _connectingConfig = _connectedConfig;
            // Don't set _connectingServer; set by each connection attempt
            newState = State::Interrupted;
            queueConnectionAttempt();
        }
        break;
    case VPNMethod::State::Exited:
        if(_method)
        {
            _method->deleteLater();
            _method = nullptr;
        }
        switch (_state)
        {
        case State::Connected:
            // Reconnect to the same location again
            _connectingConfig = _connectedConfig;
            newState = State::Interrupted;
            // Don't set _connectingServer; set by each connection attempt
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
        case State::Reconnecting:
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
            qWarning() << "VPN method exited in unexpected state" << qEnumToString(_state);
            break;
        }
        break;

    default:
        break;
    }
    setState(newState);
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
        if (_method)
            _method->shutdown();
        else
            queueConnectionAttempt();
    }
    emit error(err);
}

void VPNConnection::updateAttemptCount(int newCount)
{
    if(_connectionAttemptCount != newCount)
    {
        qInfo() << "Connection attempt count updated from"
            << _connectionAttemptCount << "to" << newCount;
        _connectionAttemptCount = newCount;
        // The slow interval flag is based on the attempt count - we don't store
        // this derived state, so it might have changed or might not.
        emit slowIntervalChanged(useSlowInterval());
    }
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
        if(state != State::Connecting && state != State::Reconnecting)
        {
            _connectionStep = ConnectionStep::Initializing;
            updateAttemptCount(0);
            _connectTimer.stop();
        }

        // In any state other than Connected, stop the resolver, even if that's
        // our current DNS setting.  (If we're reconnecting while Handshake/Local
        // DNS is selected, it'll be restarted after we connect.)
        if(state != State::Connected)
        {
            _resolverRunner.disable();
            // If it was Unbound, delete the old config file
            QFile::remove(Path::UnboundConfigFile);
            // Don't measure intervals between bytecounts
            _lastBytecountTime.clear();
        }

        _state = state;

        // Sanity-check location invariants and grab transports if they're
        // reported in this state
        nullable_t<Server> connectedServer;
        nullable_t<Transport> preferredTransport, actualTransport;
        switch(_state)
        {
        default:
            Q_ASSERT(false);
            break;
        case State::Connecting:
            Q_ASSERT(_connectingConfig.vpnLocation());
            Q_ASSERT(!_connectedConfig.vpnLocation());
            break;
        case State::Reconnecting:
            Q_ASSERT(_connectingConfig.vpnLocation());
            Q_ASSERT(_connectedConfig.vpnLocation());
            break;
        case State::Interrupted:
            Q_ASSERT(_connectingConfig.vpnLocation());
            Q_ASSERT(_connectedConfig.vpnLocation());
            break;
        case State::Connected:
            Q_ASSERT(!_connectingConfig.vpnLocation());
            Q_ASSERT(_connectedConfig.vpnLocation());
            connectedServer = _connectedServer; // Report only in Connected state
            preferredTransport = _transportSelector.lastPreferred();
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

        emit stateChanged(_state, _connectingConfig, _connectedConfig,
                          connectedServer, preferredTransport, actualTransport);
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

    if(_state == State::Connected)
    {
        // Keep track of the interval between bytecount measurements.
        // This is mainly to ensure we terminate the connection if we wake from
        // sleep.
        // Most of the time, OpenVPN and WG will do this on their own after
        // waking, because they will see there has been no activity for some
        // time.
        // However, it has been observed (at least on Mac) that data from before
        // sleep can occasionally be queued up and processed right after waking,
        // meaning the timeout resets and OpenVPN will wait another 60 seconds
        // to reconnect.  Worse, PIA says it is "connected" at this time.
        // If we don't receive any bytecounts for 4 minutes, assume the
        // connection is lost - servers are configured to time out after 1-2
        // minutes.
        if(_lastBytecountTime &&
            _lastBytecountTime->elapsed() >= bytecountAbandonLimit)
        {
            qInfo() << "Abandoning connection due to interval"
                << traceMsec(_lastBytecountTime->elapsed())
                << "between bytecount measurements (likely woke from sleep)";
            // Valid in this state; updateByteCounts() is connected to a signal from _method
            Q_ASSERT(_method);
            _method->shutdown();
        }
        else
        {
            // Measure time starting now (restart if already measuring)
            _lastBytecountTime.emplace();
        }
    }
    else
    {
        // Not connected, don't track bytecount deadline
        _lastBytecountTime.clear();
    }
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
    Q_ASSERT(successState == State::Connecting ||
             successState == State::Reconnecting ||
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
    ConnectionConfig newConfig{g_settings, g_state, g_account};

    bool changed = _connectionSettings != settings ||
        newConfig.hasChanged(_connectingConfig);

    _connectionSettings.swap(settings);
    _connectingConfig = std::move(newConfig);

    _needsReconnect = false;

    // Reset to the first attempt; settings have changed
    if(changed)
    {
        qInfo() << "Reset to first attempt, settings have changed";
        updateAttemptCount(0);
    }

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
        _connectingServer = {};
        // Go to the failure state
        setState(failureState);
        return false;
    }

    // Locations were loaded
    setState(successState);
    return true;
}

// Local 127/8 address used by a local resolver.  We can't put this on 127.0.0.1
// because port 53 may already be in use on that address.
const QString resolverLocalAddress()
{
    static QString value{QStringLiteral("127.80.73.65")};
    return value;
}
const QString piaModernDnsVpnMace()
{
    static QString value{QStringLiteral("10.0.0.241")};
    return value;
}
const QString piaModernDnsVpn()
{
    static QString value{QStringLiteral("10.0.0.243")};
    return value;
}
