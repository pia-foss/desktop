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
#line SOURCE_FILE("mac/mac_splittunnel.cpp")

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sys_domain.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <QSocketNotifier>
#include <QProcess>
#include <QThread>
#include <QtAlgorithms>
#include "posix/posix_firewall_pf.h"
#include "exec.h"
#include "mac/mac_constants.h"
#include "mac_splittunnel.h"
#include "posix/posix_objects.h"
#include "utun.h"
#include "port_finder.h" // For PortFinder
#include "daemon.h"
#include "path.h"
#include "packet.h"
namespace
{
    RegisterMetaType<QVector<QString>> qStringVector;
    RegisterMetaType<OriginalNetworkScan> qNetScan;
    RegisterMetaType<FirewallParams> qFirewallParams;

    // Unique local addresses are used (link-local addresses are not sufficient
    // as forwarded packets have no return path with a LLA since they're scoped to a link)

    // Base Ip addresses used for our Split Tunnel interface.
    // Our split tunnel device will increment these ips each time the user
    // connects to the VPN.
    const QString kSplitTunnelDeviceIpv6Base{QStringLiteral("fd00:feed:face:cafe:beef:70:69:1")};
    const QString kSplitTunnelDeviceIpv4Base{QStringLiteral("10.0.255.1")};
}

void AppCache::addEntry(IPVersion ipVersion, pid_t newPid, quint16 srcPort)
{
    if(!_cache[ipVersion].contains(newPid))
        _cache[ipVersion].insert(newPid, {});

    _cache[ipVersion][newPid] << srcPort;
}

void AppCache::refresh(IPVersion ipVersion, const OriginalNetworkScan &netScan)
{
    // Refresh the ports in our default (non-split) app cache
    for(const auto &pid : _cache[ipVersion].keys())
    {
        // Update the list of ports for each pid
        _cache[ipVersion][pid] = PortFinder::ports({pid}, ipVersion == IPv4 ? IPv4 : IPv6, netScan);
    }
}

PortSet AppCache::ports(IPVersion ipVersion) const
{
    PortSet allPorts;
    for(const auto &ports : _cache[ipVersion].values())
    {
        for(const auto &port : ports)
            allPorts << port;
    }

    return allPorts;
}

QString SplitTunnelIp::nextAddress(const QString &addressStr) const
{
    const QHostAddress address{addressStr};

    auto incrementIp = [](quint8 *pByte) {
        *pByte += 1;
        if (*pByte == 0 || *pByte == 255)
            *pByte = 1;
    };

    switch (address.protocol())
    {
    case QAbstractSocket::IPv4Protocol:
    {
        quint32 ipv4Address = address.toIPv4Address();
        quint8 *pByte = reinterpret_cast<quint8 *>(&ipv4Address);
        incrementIp(pByte);
        return QHostAddress{ipv4Address}.toString();
        break;
    }
    case QAbstractSocket::IPv6Protocol:
    {
        Q_IPV6ADDR ipv6Address = address.toIPv6Address();
        quint8 *pByte = reinterpret_cast<quint8 *>(&ipv6Address) + 15;
        incrementIp(pByte);
        return QHostAddress{ipv6Address}.toString();
        break;
    }
    default:
        qWarning() << "Invalid socket protocol"
                   << address.protocol() << "Cannot generate next split tunnel Ip";
        return {};
    }
}

// Pia Connections are special-cased.
// Source-based routing ("strong host model") no longer works when ip forwarding is enabled.
// As a result all packets (regardless of source ip) pass through our tunnel device.
//
// To route Pia connections properly we inspect the source ip
// and either add that connection to bypass or vpnOnly. We only do this with Pia connections.
// All other connections (except for those on the bypass list) are forced through the 'default'
// route which is the VPN (when connected) or the physical interface when disconnected.
class PiaConnections
{
public:
    PiaConnections(const QString &path, const MacSplitTunnel *pMacSplitTunnel);
    PortSet bypassPorts() const {return _bypassPorts;}
    PortSet vpnOnlyPorts() const {return _vpnOnlyPorts;}
private:
    PortSet _bypassPorts;
    PortSet _vpnOnlyPorts;
};

PiaConnections::PiaConnections(const QString &path, const MacSplitTunnel *pMacSplitTunnel)
{
    const auto &physAddress = pMacSplitTunnel->netScan().ipAddress();
    const auto &vpnAddress = pMacSplitTunnel->tunnelDeviceLocalAddress();

    // An address is a source-ip/source-port pair
    const auto piaAddresses = PortFinder::addresses4({path});
    // Special-case PIA connections - allow them to do what they want and route them to
    // the interface indicated by their source IP
    for(const auto &address : piaAddresses)
    {
        const auto sourceAddress = QHostAddress { address.ip() }.toString();
        if(sourceAddress == physAddress)
            _bypassPorts << address.port();
        else if(sourceAddress == vpnAddress)
            _vpnOnlyPorts << address.port();
    }
}

MacSplitTunnel::MacSplitTunnel(QObject *pParent)
: QObject{pParent}
, _bypassRuleUpdater{std::make_unique<BypassStrategy>(this), this}
, _vpnOnlyRuleUpdater{std::make_unique<VpnOnlyStrategy>(this), this}
, _defaultRuleUpdater{std::make_unique<DefaultStrategy>(this), this}
, _splitTunnelIp{kSplitTunnelDeviceIpv4Base, kSplitTunnelDeviceIpv6Base}
{
}

bool MacSplitTunnel::cycleSplitTunnelDevice()
{
    auto pNewUtun = UTun::create();

    if(!pNewUtun)
        return false;

    // Change the Ips of the new tunnel device
    // (this ensures existing connections die)
    _splitTunnelIp.refresh();

    Exec::bash(QStringLiteral("ifconfig %1 %2 %3").arg(pNewUtun->name(), _splitTunnelIp.ip4(), _splitTunnelIp.ip4()));
    Exec::bash(QStringLiteral("ifconfig %1 inet6 %2").arg(pNewUtun->name(), _splitTunnelIp.ip6()));

    // After adding the ipv4 ips to the interface we need a slight pause before trying to change the routes
    // without the pause sometimes these routes will not be created.
    // Adding the ipv6 ip is sufficient delay
    Exec::bash(QStringLiteral("route -q -n change -inet 0.0.0.0/1 -interface %1").arg(pNewUtun->name()));
    Exec::bash(QStringLiteral("route -q -n change -inet 128.0.0.0/1 -interface %1").arg(pNewUtun->name()));

    // Update IPv6 routes with new tun device
    Exec::bash(QStringLiteral("route -q -n change -inet6 8000::/1 %1").arg(_splitTunnelIp.ip6()));
    Exec::bash(QStringLiteral("route -q -n change -inet6 0::/1 %1").arg(_splitTunnelIp.ip6()));

    // Set the MTU
    pNewUtun->setMtu(_pUtun->mtu());

    // This kills the old tun device and replaces it with the new one
    _pUtun = std::move(pNewUtun);

    _readNotifier.emplace(_pUtun->fd(), QSocketNotifier::Read);
    connect(_readNotifier.ptr(), &QSocketNotifier::activated, this, &MacSplitTunnel::readFromTunnel);

    // Flush out any pre-existing firewall state
    PFFirewall::flushState();

    return true;
}

// When the user actually connects to the VPN we replace the existing stun device with a new one
// and change its Ips (both Ipv4 and Ipv6). This is so we destroy any "existing connections".
// without doing this existing connections will survive connection to the VPN as the existing connection
// has already been explicitly routed to the physical interface (via a route-to) and its source ip is still valid,
// as the stun source ip wouldn't have changed.
// By replacing the stun device on VPN connect AND changing its IP these existing connections
// will timeout and die.
void MacSplitTunnel::aboutToConnectToVpn()
{
    if(_state == State::Active)
    {
        qInfo() << "Connecting to VPN, replacing stun device and cycling Ips";

        // Replace the existing UTun with a new one so that
        // all existing TCP connections are terminated before we connect
        if(!cycleSplitTunnelDevice())
            qWarning() << "Unable to create new UTun for split tunnel on connect. Existing connections will not be cleared";

        qInfo() << "Split Tunnel ip4 addresses are" << _splitTunnelIp.ip4() << _splitTunnelIp.ip4();
        qInfo() << "Split Tunnel ip6 address is" << _splitTunnelIp.ip6();

        // Clear ipv6 rules
        _defaultRuleUpdater.clearRules(IPv6);
        _bypassRuleUpdater.clearRules(IPv6);
        _vpnOnlyRuleUpdater.clearRules(IPv6);
    }
}

void MacSplitTunnel::initiateConnection(const FirewallParams &params, QString tunnelDeviceName,
                                        QString tunnelDeviceLocalAddress)
{
    _pUtun = UTun::create();

    if(!_pUtun)
    {
        qInfo() << "Failed to open a tun device";
        return;
    }

    qInfo() << "Opened tun device" << _pUtun->name() << "for split tunnel";
    _readNotifier.emplace(_pUtun->fd(), QSocketNotifier::Read);
    connect(_readNotifier.ptr(), &QSocketNotifier::activated, this, &MacSplitTunnel::readFromTunnel);

    Exec::bash(QStringLiteral("ifconfig %1 %2 %3").arg(_pUtun->name(), _splitTunnelIp.ip4(), _splitTunnelIp.ip4()));
    // Add unique local Ipv6 IP
    Exec::bash(QStringLiteral("ifconfig %1 inet6 %2").arg(_pUtun->name(), _splitTunnelIp.ip6()));
    _pUtun->setMtu(1500); // default MTU when setting up

    // Include Ip Header when writing to raw socket
    auto hdrIncl = [](int fd) {
        int one = 1;
        setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    };

    _rawFd4 = PosixFd{socket(AF_INET, SOCK_RAW, IPPROTO_RAW)};
    // Do not inherit fds into child processes
    if(::fcntl(_rawFd4->get(), F_SETFD, FD_CLOEXEC))
        qWarning() << "fcntl failed setting FD_CLOEXEC:" << ErrnoTracer{errno};

    // Allow us to write complete Ipv4 packets, including header.
    // Ipv6 raw sockets do not allow us to do this.
    hdrIncl(_rawFd4->get());

    // Required by Big Sur
    setupIpForwarding(QStringLiteral("net.inet.ip.forwarding"), QStringLiteral("1"), _ipForwarding4);
    setupIpForwarding(QStringLiteral("net.inet6.ip6.forwarding"), QStringLiteral("1"), _ipForwarding6);

    updateSplitTunnel(params, tunnelDeviceName, tunnelDeviceLocalAddress);

    // Setup the ICMP rules
    _defaultRuleUpdater.forceUpdate(IPv4, {});
    _defaultRuleUpdater.forceUpdate(IPv6, {});

    _state = State::Active;
}

void MacSplitTunnel::shutdownConnection()
{
    _readNotifier.clear();
    _pUtun.clear();
    _routesUp = false;

    _rawFd4.clear();

    _state = State::Inactive;

    _defaultAppsCache.clearAll();
    _bypassRuleUpdater.clearAllRules();
    _vpnOnlyRuleUpdater.clearAllRules();
    _defaultRuleUpdater.clearAllRules();

    teardownIpForwarding(QStringLiteral("net.inet.ip.forwarding"), _ipForwarding4);
    teardownIpForwarding(QStringLiteral("net.inet6.ip6.forwarding"), _ipForwarding6);

    qInfo() << "Shutting down mac split tunnel";

}
void MacSplitTunnel::updateSplitTunnel(const FirewallParams &params, QString tunnelDeviceName,
                                       QString tunnelDeviceLocalAddress)
{
    updateApps(params.excludeApps, params.vpnOnlyApps);
    updateNetwork(params, tunnelDeviceName, tunnelDeviceLocalAddress);
}

static void applyExtraRules(QVector<QString> &paths)
{
    // If the system WebKit framework is excluded/vpnOnly, add this staged framework
    // path too. Newer versions of Safari use this.
    if(paths.contains(webkitFrameworkPath) &&
        !paths.contains(stagedWebkitFrameworkPath))
    {
        paths.push_back(stagedWebkitFrameworkPath);
    }

    // Adding elements to paths may cause it to reallocate (invalidating all
    // iterators); iterate using an index.
    for(int i=0; i<paths.size(); ++i)
    {
        if(paths[i].contains(QStringLiteral("/App Store.app"), Qt::CaseInsensitive)) {
            paths.push_back(QStringLiteral("/System/Library/PrivateFrameworks/AppStoreDaemon.framework/Support/appstoreagent"));
        }
        else if(paths[i].contains(QStringLiteral("/Calendar.app"), Qt::CaseInsensitive)) {
            paths.push_back(QStringLiteral("/System/Library/PrivateFrameworks/CalendarAgent.framework/Executables/CalendarAgent"));
        }
        else if(paths[i].contains(QStringLiteral("/Safari.app"), Qt::CaseInsensitive)) {
            paths.push_back(QStringLiteral("/System/Library/CoreServices/SafariSupport.bundle/Contents/MacOS/SafariBookmarksSyncAgent"));
            paths.push_back(QStringLiteral("/System/Library/StagedFrameworks/Safari/WebKit.framework/Versions/A/XPCServices/com.apple.WebKit.Networking.xpc"));
            paths.push_back(QStringLiteral("/System/Library/PrivateFrameworks/SafariSafeBrowsing.framework/Versions/A/com.apple.Safari.SafeBrowsing.Service"));
            paths.push_back(QStringLiteral("/System/Library/StagedFrameworks/Safari/SafariShared.framework/Versions/A/XPCServices/com.apple.Safari.SearchHelper.xpc"));
        }
    }
}

void MacSplitTunnel::updateApps(QVector<QString> excludedApps, QVector<QString> vpnOnlyApps)
{
    if(_state == State::Inactive)
    {
        qWarning() << "Cannot update excluded apps, not connected to split tunnel device";
        return;
    }

    // Possibly modify vpnOnly/exclusions vector to handle webkit/safari apps
    applyExtraRules(excludedApps);
    applyExtraRules(vpnOnlyApps);

    // Ensure PIA executables bypass the VPN
    excludedApps.push_back(Path::ExecutableDir);

    // If nothing has changed, just return
    if(_excludedApps != excludedApps)
    {
        _excludedApps = std::move(excludedApps);
        for(const auto &app : _excludedApps) qInfo() << "Excluded Apps:" << app;
    }

    if(_vpnOnlyApps != vpnOnlyApps)
    {
        _vpnOnlyApps = std::move(vpnOnlyApps);
        for(const auto &app : _vpnOnlyApps) qInfo() << "VPN Only Apps:" << app;
    }

    qInfo() << "Updated apps";
}

QString MacSplitTunnel::sysctl(const QString &setting, const QString &value)
{
    QString out = Exec::bashWithOutput(QStringLiteral("sysctl -n '%1'").arg(setting));

    if(out.isEmpty())
    {
        qWarning() << QStringLiteral("Unable to read value of %1 setting from sysctl").arg(setting);
        return {};
    }

    if(out.toInt() != value.toInt())
    {
        qInfo() << QStringLiteral("Setting %1 to %2").arg(setting, value);
        if(0 == Exec::bash(QStringLiteral("sysctl -w '%1=%2'").arg(setting, value)))
            return out;
    }
    else
    {
        qInfo() << QStringLiteral("%1 already set to %2; nothing to do!").arg(setting).arg(value);
        return {};
    }

    qWarning() << QStringLiteral("Unable to set old %1 value from %2 to %3").arg(setting).arg(out).arg(value);

    return {};
}

// setupIpForwarding(QStringLiteral("net.inet.ip.forwarding"), QStringLiteral("1"), _ipForwarding4);
void MacSplitTunnel::setupIpForwarding(const QString &setting, const QString &value, QString &storedValue)
{
    auto oldValue = sysctl(setting, value);
    if(!oldValue.isEmpty())
    {
        qInfo() << QStringLiteral("Storing old %1 value: %2").arg(setting).arg(oldValue);
        storedValue = oldValue;
    }
}

// teardownIpForwarding(QStringLiteral("net.inet.ip.forwarding"), _ipForwarding4);
void MacSplitTunnel::teardownIpForwarding(const QString &setting, QString &storedValue)
{
    qInfo() << "Tearing down ip forwarding, stored value is" << storedValue;
    if(!storedValue.isEmpty())
    {
        sysctl(setting, storedValue);
        storedValue = "";
    }
}

void MacSplitTunnel::updateNetwork(const FirewallParams &params, QString tunnelDeviceName,
                                   QString tunnelDeviceLocalAddress)
{
    qInfo() << "Updating Network info for mac split tunnel";

    // Create a bound route for the VPN interface, so sockets explicitly bound
    // to that interface will work (they could be bound by the daemon or bound
    // by an app that is configured to use the VPN interface).
    //
    // The VPN interface is never the default when Mac split tunnel is enabled,
    // so this route is always needed (the split tunnel device gets the default
    // route).
    if(tunnelDeviceName.isEmpty())
    {
        // We don't know the tunnel interface info right away; these are found
        // later.  (Split tunnel is started immediately to avoid blips for apps
        // that bypass the VPN.)
        qInfo() << "Can't create bound route for VPN network yet - interface not known";
    }
    else
    {
        // We don't need to remove the previous bound route as if the IP
        // changes that means the interface went down, which will destroy
        // that route.
        // Create a direct interface route  - this is fine for tunnel
        // devices since they're point to point so there will only be one
        // acceptable "next hop"
        Exec::bash(QStringLiteral("route add -net 0.0.0.0 -interface %1 -ifscope %1").arg(tunnelDeviceName));
    }

    if(params._connectionSettings && params._connectionSettings->mtu())
    {
        auto mtu = params._connectionSettings->mtu();
        if (_pUtun->mtu() != mtu)
            _pUtun->setMtu(mtu);
    }
    else
    {
        // 1420 is the default MTU for the VPN tunnel device
        if(_pUtun->mtu() != 1420)
            _pUtun->setMtu(1420);
    }

    // Create routes into the split tunnel device to override the default
    // gateway.
    // The VPN normally is not set as the default gateway when split tunnel is
    // active on Mac.  This only happens if split tunnel is enabled while
    // connected; in that case we can start up split tunnel, but we have to wait
    // for a reconnect to update routes.
    if(!params.setDefaultRoute)
    {
        // IPv4 default routes
        Exec::bash(QStringLiteral("route -q -n add -inet 0.0.0.0/1 -interface %1").arg(_pUtun->name()));
        Exec::bash(QStringLiteral("route -q -n add -inet 128.0.0.0/1 -interface %1").arg(_pUtun->name()));

        // IPv6 default routes - (equivalent to 128/1 route for ipv6)
        Exec::bash(QStringLiteral("route -q -n add -inet6 8000::/1 %1").arg(_splitTunnelIp.ip6()));
        // Equivalent to 0/1 route for ipv4
        Exec::bash(QStringLiteral("route -q -n add -inet6 0::/1 %1").arg(_splitTunnelIp.ip6()));

        _routesUp = true;
    }
    else
    {
        qInfo() << "Can't create split tunnel device routes yet, VPN is still default route - wait for reconnect";
    }

    // If we're disconnected, ensure we cycle the st device if killswitch changes.
    // This is so when we go from ks:auto/off -> ks:always we break existing connections.
    // Without  this, existing connections will continue to exist when ks is toggled to always
    if(params.hasConnected == false && params.blockAll != _params.blockAll)
        cycleSplitTunnelDevice();

    // Update our network info
    _params = params;
    _tunnelDeviceName = tunnelDeviceName;
    _tunnelDeviceLocalAddress = tunnelDeviceLocalAddress;
}

bool MacSplitTunnel::isSplitPort(quint16 port,
                                 const PortSet &bypassPorts,
                                 const PortSet &vpnOnlyPorts)
{
    return bypassPorts.contains(port) || vpnOnlyPorts.contains(port);
}

void MacSplitTunnel::handleIp6(std::vector<unsigned char> buffer, int actualSize)
{
    // skip the first 4 bytes (it stores AF_NET)
    const auto pPacket = Packet6::createFromData(std::move(buffer), 4);
    if(!pPacket)
    {
        qWarning() << "Packet is invalid; read" << actualSize << "bytes from utun";
        return;
    }

    if(_params.isConnected)
    {
        // Do not allow any ipv6 packets when connected
        // as we don't (yet) support ipv6 and the packets would just clutter
        // the network.
        _defaultRuleUpdater.clearRules(IPv6);
        _bypassRuleUpdater.clearRules(IPv6);
        _vpnOnlyRuleUpdater.clearRules(IPv6);

        return;
    }

    // Update the cache for non-split apps, to keep track of the ports we care about
    // when generating firewall rules
    _defaultAppsCache.refresh(IPv6, netScan());

    // Get ports for our tracked apps
    auto bypassPorts = PortFinder::ports(_excludedApps, IPv6, netScan());
    auto vpnOnlyPorts = PortFinder::ports(_vpnOnlyApps, IPv6, netScan());
    auto defaultPorts = _defaultAppsCache.ports(IPv6);

    // These packets seem to have protocol 255, so drop them
    if(pPacket->packetType() == Packet6::Other)
        return;

        // Drop vpnOnly packets when not connected
    if(!_params.isConnected && pPacket->sourcePort() && vpnOnlyPorts.contains(pPacket->sourcePort()))
    {
        qInfo() << "Dropping an Ipv6 vpnOnly packet";
        return;
    }

    // We only add a (non-split) app cache entry if the port wasn't associated with
    // a bypass or vpnonly app
    if(pPacket->packetType() != Packet6::Other && !isSplitPort(pPacket->sourcePort(), bypassPorts, vpnOnlyPorts))
    {
        pid_t newPid = PortFinder::pidForPort(pPacket->sourcePort(), IPv6);
        if(newPid)
            _defaultAppsCache.addEntry(IPv6, newPid, pPacket->sourcePort());
        else
            // We could not find an associated PID for the packet, so drop it.
            // We drop a packet by just returning since a packet only goes further if it's re-injected
            return;
    }

    // Drop multicast and self-addressed packets
    // broadcast packets don't exist on ipv6
    const auto destAddress = QHostAddress { reinterpret_cast<const quint8*>(&pPacket->destAddress()) };
    if(destAddress.isMulticast() || destAddress.toString() == _splitTunnelIp.ip6())
        return; // We drop a packet by just returnin

    // Prevent default traffic when KS=always and disconnected
    // All other traffic is fine - vpnOnly is blocked anyway and bypass is allowed
    if(_params.blockAll && !_params.isConnected)
        defaultPorts.clear();

    if(_flowTracker.track(*pPacket) == FlowTracker::RepeatedFlow)
    {
        qInfo() << "Observed repeated packet (> 10 times), dropping" << pPacket->toString();
        return;
    }

    _defaultRuleUpdater.update(IPv6, defaultPorts);
    _bypassRuleUpdater.update(IPv6, bypassPorts);
    _vpnOnlyRuleUpdater.update(IPv6, vpnOnlyPorts);

    // Re-inject the packet
    // Left out for now as IPv6 packet re-injection doesn't
    // work as IPv6 packets are not injected with IP headers intact
    // Instead we rely on TCP retrying the packet send after the pf rule is setup
    // UDP is not properly supported.
    // TODO: look into data link layer IPv6 injection via PF_NDRV sockets
}

void MacSplitTunnel::handleIp4(std::vector<unsigned char> buffer, int actualSize)
{
    // skip the first 4 bytes (it stores AF_NET)
    const auto pPacket = Packet::createFromData(std::move(buffer), 4);
    if(!pPacket)
    {
        qWarning() << "Packet is invalid; read" << actualSize << "bytes from stun";
        return;
    }

    PiaConnections piaConnections{Path::ExecutableDir, this};

    // Update the cache for non-split apps, to keep track of the ports we care about
    // when generating firewall rules
    _defaultAppsCache.refresh(IPv4, netScan());

    // Get ports for our tracked apps
    auto bypassPorts = PortFinder::ports(_excludedApps, IPv4, netScan());
    auto vpnOnlyPorts = PortFinder::ports(_vpnOnlyApps, IPv4, netScan());
    auto defaultPorts = _defaultAppsCache.ports(IPv4);

    // These packets seem to have protocol 255, so drop them
    if(pPacket->packetType() == Packet::Other)
        return;

    // Update with our pia-specific connections
    bypassPorts += piaConnections.bypassPorts();
    vpnOnlyPorts += piaConnections.vpnOnlyPorts();

    // Drop vpnOnly packets when not connected
    if(!_params.isConnected && pPacket->sourcePort() && vpnOnlyPorts.contains(pPacket->sourcePort()))
    {
        qInfo() << "Dropping an Ipv4 vpnOnly packet";
        return;
    }

    // We only add a (non-split) app cache entry if the port wasn't associated with
    // a bypass or vpnonly app
    if(pPacket->packetType() != Packet::Other && !isSplitPort(pPacket->sourcePort(), bypassPorts, vpnOnlyPorts))
    {
        pid_t newPid = PortFinder::pidForPort(pPacket->sourcePort(), IPv4);
        if(newPid)
            _defaultAppsCache.addEntry(IPv4, newPid, pPacket->sourcePort());
        else
            // We could not find an associated PID for the packet, so drop it.
            // We drop a packet by just returning since a packet only goes further if it's re-injected
            return;
    }

    // Drop multicast/broadcast and self-addressed packets
    const auto destAddress = QHostAddress { pPacket->destAddress() };
    if(destAddress.isMulticast() || destAddress.isBroadcast() || destAddress.toString() == _splitTunnelIp.ip4())
        return; // We drop a packet by just returning

    if(_flowTracker.track(*pPacket) == FlowTracker::RepeatedFlow)
    {
        qInfo() << "Observed repeated packet (> 10 times), dropping" << pPacket->toString();
        return;
    }

    // Prevent default traffic when KS=always and disconnected
    // All other traffic is fine - vpnOnly is blocked anyway and bypass is allowed
    if(_params.blockAll && !_params.isConnected)
    {
        defaultPorts.clear();
    }

    _defaultRuleUpdater.update(IPv4, defaultPorts);
    _bypassRuleUpdater.update(IPv4, bypassPorts);
    _vpnOnlyRuleUpdater.update(IPv4, vpnOnlyPorts);

    //qInfo() << "Re-injecting IPv4 packet:" << pPacket->toString();

    // Re-inject the packet
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(pPacket->destAddress());

    if(::sendto(_rawFd4->get(), pPacket->toRaw(), pPacket->len() , 0, reinterpret_cast<sockaddr *>(&to), sizeof(to)) == -1)
    {
        qWarning() << "Unable to reinject packet" << pPacket->toString() << "-"
            << ErrnoTracer{errno};
        qWarning() << "Packet -" << pPacket->len() << "bytes";
        std::uint32_t *pPktWords = reinterpret_cast<std::uint32_t*>(pPacket->toRaw());
        for(int i=0; i+4 <= pPacket->len(); i += 4)
        {
            qWarning() << QString::asprintf("%03d", i) << QString::asprintf("%08X", pPktWords[i/4]);
        }
        if(pPacket->len() % 4)
        {
            std::uint8_t *pTailBytes = reinterpret_cast<std::uint8_t*>(pPacket->toRaw());
            unsigned lastWordOffset = pPacket->len() / 4;
            pTailBytes += lastWordOffset * 4;
            std::uint32_t lastWord = 0;
            lastWord |= pTailBytes[0];
            lastWord <<= 8;
            if(pPacket->len() % 4 >= 2)
                lastWord |= pTailBytes[1];
            lastWord <<= 8;
            if(pPacket->len() % 4 >= 3)
                lastWord |= pTailBytes[2];
            lastWord <<= 8;
            qWarning() << QString::asprintf("%03d", lastWordOffset*4) << QString::asprintf("%08X", lastWord);
        }
    }
}

void MacSplitTunnel::readFromTunnel(int socket)
{
    // + 4 bytes for the address family
    const uint mtuPlusHeader = _pUtun->mtu() + 4;

    std::vector<unsigned char> buffer(mtuPlusHeader);

    ssize_t actual = ::read(socket, buffer.data(), mtuPlusHeader);
    buffer.resize(actual);
    if(actual == -1)
    {
        qWarning() << "Unable to read from split tunnel device:" << ErrnoTracer{errno};
        return;
    }

    // First 4 bytes indicate address family (IPv4 or IPv6)
    int addressFamily = ntohl(*reinterpret_cast<int *>(buffer.data()));

    switch(addressFamily)
    {
    case AF_INET:
        handleIp4(std::move(buffer), actual);
        break;
    case AF_INET6:
        handleIp6(std::move(buffer), actual);
        break;
    default:
        qWarning() << "Unsupported address family:" << addressFamily;
    }
 }
