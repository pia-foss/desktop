// Copyright (c) 2023 Private Internet Access, Inc.
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

#include "mac_splittunnel.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <kapps_core/src/logger.h>
#include <arpa/inet.h>
#include <fcntl.h> // For raw sockets
#include "pf_firewall.h"
#include <kapps_core/src/ipaddress.h>
#include <kapps_core/src/mac/mac_constants.h>
#include "utun.h"
#include "port_finder.h"
#include "packet.h"

namespace
{
    // Unique local addresses are used (link-local addresses are not sufficient
    // as forwarded packets have no return path with a LLA since they're scoped to a link)

    // Base Ip addresses used for our Split Tunnel interface.
    // Our split tunnel device will increment these ips each time the user
    // connects to the VPN.
    const std::string kSplitTunnelDeviceIpv6Base{"fd00:feed:face:cafe:beef:70:69:1"};
    const std::string kSplitTunnelDeviceIpv4Base{"10.0.255.1"};

    template <typename C, typename V>
    bool contains(const C &container, const V &value)
    {
        return std::find(std::begin(container), std::end(container), value) != std::end(container);
    }
}

namespace kapps { namespace net {

void AppCache::addEntry(IPVersion ipVersion, pid_t newPid, std::uint16_t srcPort)
{
    if(!_cache[ipVersion].count(newPid))
        _cache[ipVersion].insert({newPid, {}});

    _cache[ipVersion][newPid].insert(srcPort);
}

void AppCache::refresh(IPVersion ipVersion, const OriginalNetworkScan &netScan)
{
    // Refresh the ports in our default (non-split) app cache
    for(const auto &pair : _cache[ipVersion])
    {
        // Update the list of ports for each pid
        const pid_t pid = pair.first;
        _cache[ipVersion][pid] = PortFinder::ports({pid}, ipVersion == IPv4 ? IPv4 : IPv6, netScan);
    }
}

PortSet AppCache::ports(IPVersion ipVersion) const
{
    PortSet allPorts;
    for(const auto &pair : _cache[ipVersion])
    {
        const PortSet &portSet{pair.second};
        for(const auto &port : portSet)
            allPorts.insert(port);
    }

    return allPorts;
}

std::string SplitTunnelIp::nextAddress(const std::string &addressStr, IPVersion ipVersion) const
{
    auto incrementIp = [](std::uint8_t *pByte) {
        *pByte += 1;
        if (*pByte == 0 || *pByte == 255)
            *pByte = 1;
    };

    switch(ipVersion)
    {
    case IPv4:
    {
        core::Ipv4Address address{addressStr};
        std::uint32_t ipv4Address = address.address();
        std::uint8_t *pByte = reinterpret_cast<std::uint8_t *>(&ipv4Address);
        incrementIp(pByte);
        return core::Ipv4Address{ipv4Address}.toString();
    }
    case IPv6:
    {
        core::Ipv6Address address{addressStr};
        core::Ipv6Address::AddressValue ipv6Address;
        std::copy(std::begin(address.address()), std::end(address.address()), std::begin(ipv6Address));
        std::uint8_t *pByte = reinterpret_cast<std::uint8_t *>(&ipv6Address) + 15;
        incrementIp(pByte);
        return core::Ipv6Address{ipv6Address}.toString();
    }
    default:
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
    PiaConnections(const std::string &path, const std::string &vpnAddress,
                   const std::string &physAddress);
    const PortSet &bypassPorts() const {return _bypassPorts;}
    const PortSet &vpnOnlyPorts() const {return _vpnOnlyPorts;}
    bool isPiaPort(uint16_t port) const;
private:
    PortSet _bypassPorts;
    PortSet _vpnOnlyPorts;
};

bool PiaConnections::isPiaPort(uint16_t port) const
{
    return _bypassPorts.count(port) || _vpnOnlyPorts.count(port);
}

PiaConnections::PiaConnections(const std::string &path, const std::string &vpnAddress,
                               const std::string &physAddress)
{
    // An address is a source-ip/source-port pair
    const auto piaAddresses = PortFinder::addresses4({path});
    // Special-case PIA connections - allow them to do what they want and route them to
    // the interface indicated by their source IP
    for(const auto &address : piaAddresses)
    {
        const auto sourceAddress = core::Ipv4Address { address.ip() }.toString();
        if(sourceAddress == physAddress)
            _bypassPorts.insert(address.port());
        else if(sourceAddress == vpnAddress)
            _vpnOnlyPorts.insert(address.port());
    }
}

MacSplitTunnel::MacSplitTunnel(const FirewallParams &params, const std::string &executableDir, PFFirewall &filter)
: _filter{filter}, _bypassRuleUpdater{std::make_unique<BypassStrategy>(filter), filter}
, _vpnOnlyRuleUpdater{std::make_unique<VpnOnlyStrategy>(filter), filter}
, _defaultRuleUpdater{std::make_unique<DefaultStrategy>(filter), filter}
, _splitTunnelIp{kSplitTunnelDeviceIpv4Base, kSplitTunnelDeviceIpv6Base}
, _executableDir{executableDir}
{
    assert(!_executableDir.empty());
    _utunNotifier.activated = [this]{readFromSocket();};
    initiateConnection(params);
}

bool MacSplitTunnel::cycleSplitTunnelDevice()
{
    auto pNewUtun = UTun::create();

    if(!pNewUtun)
        return false;

    // Change the Ips of the new tunnel device
    // (this ensures existing connections die)
    _splitTunnelIp.refresh();

    kapps::core::Exec::bash(qs::format("ifconfig % % %", pNewUtun->name(), _splitTunnelIp.ip4(), _splitTunnelIp.ip4()));
    kapps::core::Exec::bash(qs::format("ifconfig % inet6 %", pNewUtun->name(), _splitTunnelIp.ip6()));

    // After adding the ipv4 ips to the interface we need a slight pause before trying to change the routes
    // without the pause sometimes these routes will not be created.
    // Adding the ipv6 ip is sufficient delay
    kapps::core::Exec::bash(qs::format("route -q -n change -inet 0.0.0.0/1 -interface %", pNewUtun->name()));
    kapps::core::Exec::bash(qs::format("route -q -n change -inet 128.0.0.0/1 -interface %", pNewUtun->name()));

    // Update IPv6 routes with new tun device
    kapps::core::Exec::bash(qs::format("route -q -n change -inet6 8000::/1 %", _splitTunnelIp.ip6()));
    kapps::core::Exec::bash(qs::format("route -q -n change -inet6 0::/1 %", _splitTunnelIp.ip6()));

    // Set the MTU
    pNewUtun->setMtu(_pUtun->mtu());

    _utunNotifier.cancel();
    // This kills the old tun device and replaces it with the new one
    _pUtun = std::move(pNewUtun);
    _utunNotifier.set(_pUtun->fd(), kapps::core::PosixFdNotifier::WatchType::Read);

    // Flush out any pre-existing firewall state
    _filter.flushState();

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
        KAPPS_CORE_INFO() << "Connecting to VPN, replacing stun device and cycling Ips";

        // Replace the existing UTun with a new one so that
        // all existing TCP connections are terminated before we connect
        if(!cycleSplitTunnelDevice())
            KAPPS_CORE_WARNING() << "Unable to create new UTun for split tunnel on connect. Existing connections will not be cleared";

        KAPPS_CORE_INFO() << "Split Tunnel ip4 addresses are" << _splitTunnelIp.ip4() << _splitTunnelIp.ip4();
        KAPPS_CORE_INFO() << "Split Tunnel ip6 address is" << _splitTunnelIp.ip6();

        // Clear ipv6 rules
        _defaultRuleUpdater.clearRules(IPv6);
        _bypassRuleUpdater.clearRules(IPv6);
        _vpnOnlyRuleUpdater.clearRules(IPv6);
    }
}

void MacSplitTunnel::initiateConnection(const FirewallParams &params)
{
    _pUtun = UTun::create();

    if(!_pUtun)
    {
        KAPPS_CORE_INFO() << "Failed to open a tun device";
        return;
    }

    _utunNotifier.set(_pUtun->fd(), kapps::core::PosixFdNotifier::WatchType::Read);

    KAPPS_CORE_INFO() << "Opened tun device" << _pUtun->name() << "for split tunnel";

    kapps::core::Exec::bash(qs::format("ifconfig % % %", _pUtun->name(), _splitTunnelIp.ip4(), _splitTunnelIp.ip4()));
    // Add unique local Ipv6 IP
    kapps::core::Exec::bash(qs::format("ifconfig % inet6 %", _pUtun->name(), _splitTunnelIp.ip6()));
    _pUtun->setMtu(1500); // default MTU when setting up

    // Include Ip Header when writing to raw socket
    auto hdrIncl = [](int fd) {
        int one = 1;
        setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    };

    _rawFd4 = kapps::core::PosixFd{socket(AF_INET, SOCK_RAW, IPPROTO_RAW)};
    // Do not inherit fds into child processes
    if(::fcntl(_rawFd4->get(), F_SETFD, FD_CLOEXEC))
        KAPPS_CORE_WARNING() << "fcntl failed setting FD_CLOEXEC:" << kapps::core::ErrnoTracer{errno};

    // Allow us to write complete Ipv4 packets, including header.
    // Ipv6 raw sockets do not allow us to do this.
    hdrIncl(_rawFd4->get());

    // Required by Big Sur
    setupIpForwarding("net.inet.ip.forwarding", "1", _ipForwarding4);
    setupIpForwarding("net.inet6.ip6.forwarding", "1", _ipForwarding6);

    _state = State::Active;

    updateSplitTunnel(params);

    // Setup the ICMP rules
    _defaultRuleUpdater.forceUpdate(IPv4, {}, params);
    _defaultRuleUpdater.forceUpdate(IPv6, {}, params);
}

void MacSplitTunnel::shutdownConnection()
{
    //_readNotifier.clear();
    _pUtun.clear();
    _routesUp = false;

    _rawFd4.clear();

    _state = State::Inactive;

    _defaultAppsCache.clearAll();
    _bypassRuleUpdater.clearAllRules();
    _vpnOnlyRuleUpdater.clearAllRules();
    _defaultRuleUpdater.clearAllRules();

    teardownIpForwarding("net.inet.ip.forwarding", _ipForwarding4);
    teardownIpForwarding("net.inet6.ip6.forwarding", _ipForwarding6);

    KAPPS_CORE_INFO() << "Shutting down mac split tunnel";

}
void MacSplitTunnel::updateSplitTunnel(const FirewallParams &params)
{
    updateApps(params.excludeApps, params.vpnOnlyApps);
    updateNetwork(params);
}

void MacSplitTunnel::readFromSocket()
{
    if(!_pUtun)
        return;

    // Read incoming packets and process them.  Reading from a utun device
    // provides one packet at a time.
    //
    // There's practically no documentation for Darwin utun devices, but
    // assuming it's like the BSD/Linux tun devices, it'll silently truncate
    // packets if our receive buffer is not large enough.  Since the address
    // family is prepended to the packet, we should read at least MTU+4 bytes.
    //
    // Process up to 100 packets per wake-up.  We don't want to return to
    // poll(2) for every single packet if a lot are available, as it impacts
    // throughput.  However, we also can't process all packets until none
    // remain, as an app could keep us busy in this loop by continuing to send
    // packets - and the thread would be unable to process work items, such as
    // reconfiguring split tunnel.
    //
    // This is a throughput/latency tradeoff.  We want to make sure the thread
    // can respond to work items reasonably quickly, while also not impacting
    // throughput excessively.  Actually measuring the elasped time might be
    // reasonable here too.

    auto mtu = _pUtun->mtu();
    int processedCount{0};
    while(++processedCount <= 100)
    {
        std::vector<unsigned char> buffer;
        // Extra uint32 for the address family
        buffer.resize(mtu + sizeof(std::uint32_t));

        ssize_t actual{};
        NO_EINTR(actual = ::read(_pUtun->fd(), buffer.data(), buffer.size()));
        if(actual < 0)
        {
            // EWOULDBLOCK is normal and indicates there's no data left; trace
            // anything else.
            if(errno != EWOULDBLOCK)
            {
                KAPPS_CORE_WARNING() << "Failed to read from split tunnel device:"
                    << core::ErrnoTracer{errno};
            }
            break;  // We're done, nothing left to read
        }

        buffer.resize(static_cast<std::size_t>(actual));
        // Got a packet
        handleTunnelPacket(std::move(buffer));
    }
}

static void applyExtraRules(std::vector<std::string> &paths)
{
    // Case insensitive startsWith
    auto iStartsWith = [](std::string str, std::string substr)
    {
        // convert string to lowercase
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        // convert substring to lowercase
        std::transform(substr.begin(), substr.end(), substr.begin(), ::tolower);

        // does str start with substr?
        return str.rfind(substr, 0) == 0;
    };

    // If the system WebKit framework is excluded/vpnOnly, add this staged framework
    // path too. Newer versions of Safari use this.
    if(contains(paths, webkitFrameworkPath) &&
        !contains(paths, stagedWebkitFrameworkPath))
    {
        paths.push_back(stagedWebkitFrameworkPath);
    }

    // Adding elements to paths may cause it to reallocate (invalidating all
    // iterators); iterate using an index.
    for(size_t i=0; i<paths.size(); ++i)
    {

        if(iStartsWith(paths[i], "/App Store.app")) {
            paths.emplace_back("/System/Library/PrivateFrameworks/AppStoreDaemon.framework/Support/appstoreagent");
        }
        else if(iStartsWith(paths[i], "/Calendar.app")) {
            paths.emplace_back("/System/Library/PrivateFrameworks/CalendarAgent.framework/Executables/CalendarAgent");
        }
        else if(iStartsWith(paths[i], "/Safari.app")) {
            paths.emplace_back("/System/Library/CoreServices/SafariSupport.bundle/Contents/MacOS/SafariBookmarksSyncAgent");
            paths.emplace_back("/System/Library/StagedFrameworks/Safari/WebKit.framework/Versions/A/XPCServices/com.apple.WebKit.Networking.xpc");
            paths.emplace_back("/System/Library/PrivateFrameworks/SafariSafeBrowsing.framework/Versions/A/com.apple.Safari.SafeBrowsing.Service");
            paths.emplace_back("/System/Library/StagedFrameworks/Safari/SafariShared.framework/Versions/A/XPCServices/com.apple.Safari.SearchHelper.xpc");
        }
    }
}

void MacSplitTunnel::updateApps(std::vector<std::string> excludedApps, std::vector<std::string> vpnOnlyApps)
{
    if(_state == State::Inactive)
    {
        KAPPS_CORE_WARNING() << "Cannot update excluded apps, not connected to split tunnel device";
        return;
    }

    // Possibly modify vpnOnly/exclusions vector to handle webkit/safari apps
    applyExtraRules(excludedApps);
    applyExtraRules(vpnOnlyApps);

    //excludedApps.push_back(_executableDir);

    // If nothing has changed, just return
    if(_excludedApps != excludedApps)
    {
        _excludedApps = std::move(excludedApps);
        for(const auto &app : _excludedApps) KAPPS_CORE_INFO() << "Excluded Apps:" << app;
    }

    if(_vpnOnlyApps != vpnOnlyApps)
    {
        _vpnOnlyApps = std::move(vpnOnlyApps);
        for(const auto &app : _vpnOnlyApps) KAPPS_CORE_INFO() << "VPN Only Apps:" << app;
    }

    KAPPS_CORE_INFO() << "Updated apps";
}

std::string MacSplitTunnel::sysctl(const std::string &setting, const std::string &value)
{
    std::string out = kapps::core::Exec::bashWithOutput(qs::format("sysctl -n '%'", setting));

    if(out.empty())
    {
        KAPPS_CORE_WARNING() << qs::format("Unable to read value of % setting from sysctl", setting);
        return {};
    }

    if(out != value)
    {
        KAPPS_CORE_INFO() << qs::format("Setting % to %", setting, value);
        if(0 == kapps::core::Exec::bash(qs::format("sysctl -w '%=%'", setting, value)))
            return out;
    }
    else
    {
        KAPPS_CORE_INFO() << qs::format("% already set to %; nothing to do!", setting, value);
        return {};
    }

    KAPPS_CORE_WARNING() << qs::format("Unable to set old % value from % to %", setting, out, value);

    return {};
}

// setupIpForwarding(QStringLiteral("net.inet.ip.forwarding"), QStringLiteral("1"), _ipForwarding4);
void MacSplitTunnel::setupIpForwarding(const std::string &setting, const std::string &value, std::string &storedValue)
{
    auto oldValue = sysctl(setting, value);
    if(!oldValue.empty())
    {
        KAPPS_CORE_INFO() << qs::format("Storing old % value: %", setting, oldValue);
        storedValue = oldValue;
    }
}

// teardownIpForwarding(QStringLiteral("net.inet.ip.forwarding"), _ipForwarding4);
void MacSplitTunnel::teardownIpForwarding(const std::string &setting, std::string &storedValue)
{
    KAPPS_CORE_INFO() << "Tearing down ip forwarding, stored value is" << storedValue;
    if(!storedValue.empty())
    {
        sysctl(setting, storedValue);
        storedValue = "";
    }
}

void MacSplitTunnel::updateNetwork(const FirewallParams &params)
{
    KAPPS_CORE_INFO() << "Updating Network info for mac split tunnel";

    // Create a bound route for the VPN interface, so sockets explicitly bound
    // to that interface will work (they could be bound by the daemon or bound
    // by an app that is configured to use the VPN interface).
    //
    // The VPN interface is never the default when Mac split tunnel is enabled,
    // so this route is always needed (the split tunnel device gets the default
    // route).
    if(params.tunnelDeviceName.empty())
    {
        // We don't know the tunnel interface info right away; these are found
        // later.  (Split tunnel is started immediately to avoid blips for apps
        // that bypass the VPN.)
        KAPPS_CORE_INFO() << "Can't create bound route for VPN network yet - interface not known";
    }
    else
    {
        // We don't need to remove the previous bound route as if the IP
        // changes that means the interface went down, which will destroy
        // that route.
        // Create a direct interface route  - this is fine for tunnel
        // devices since they're point to point so there will only be one
        // acceptable "next hop"
        kapps::core::Exec::bash(qs::format("route add -net 0.0.0.0 -interface % -ifscope %", params.tunnelDeviceName, params.tunnelDeviceName));
    }

    if(params.mtu > 0)
    {
        if (_pUtun->mtu() != params.mtu)
            _pUtun->setMtu(params.mtu);
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
        kapps::core::Exec::bash(qs::format("route -q -n add -inet 0.0.0.0/1 -interface %", _pUtun->name()));
        kapps::core::Exec::bash(qs::format("route -q -n add -inet 128.0.0.0/1 -interface %", _pUtun->name()));

        // IPv6 default routes - (equivalent to 128/1 route for ipv6)
        kapps::core::Exec::bash(qs::format("route -q -n add -inet6 8000::/1 %", _splitTunnelIp.ip6()));
        // Equivalent to 0/1 route for ipv4
        kapps::core::Exec::bash(qs::format("route -q -n add -inet6 0::/1 %", _splitTunnelIp.ip6()));

        _routesUp = true;
    }
    else
    {
        KAPPS_CORE_INFO() << "Can't create split tunnel device routes yet, VPN is still default route - wait for reconnect";
    }

    // If we're disconnected, ensure we cycle the st device if killswitch changes.
    // This is so when we go from ks:auto/off -> ks:always we break existing connections.
    // Without  this, existing connections will continue to exist when ks is toggled to always
    if(params.hasConnected == false && params.blockAll != _params.blockAll)
    {
        // Remove until fix bug related to delayed messages
        // cycleSplitTunnelDevice();
    }

    // Update our network info
    _params = params;
}

bool MacSplitTunnel::isSplitPort(std::uint16_t port,
                                 const PortSet &bypassPorts,
                                 const PortSet &vpnOnlyPorts)
{
    return contains(bypassPorts, port) || contains(vpnOnlyPorts, port);
}

void MacSplitTunnel::handleIp6(std::vector<unsigned char> buffer)
{
    // Grab size for tracing in case the packet is invalid, since we move from buffer
    auto actualSize = buffer.size();
    // skip the first 4 bytes (it stores AF_NET)
    const auto pPacket = Packet6::createFromData(std::move(buffer), 4);
    if(!pPacket)
    {
        KAPPS_CORE_WARNING() << "Packet is invalid; read" << actualSize << "bytes from utun";
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
    _defaultAppsCache.refresh(IPv6, _params.netScan);

    // Get ports for our tracked apps
    auto bypassPorts = PortFinder::ports(_excludedApps, IPv6, _params.netScan);
    auto vpnOnlyPorts = PortFinder::ports(_vpnOnlyApps, IPv6, _params.netScan);
    auto defaultPorts = _defaultAppsCache.ports(IPv6);

    // These packets seem to have protocol 255, so drop them
    if(pPacket->packetType() == Packet6::Other)
        return;

        // Drop vpnOnly packets when not connected
    if(!_params.isConnected && pPacket->sourcePort() && contains(vpnOnlyPorts, pPacket->sourcePort()))
    {
        KAPPS_CORE_INFO() << "Dropping an Ipv6 vpnOnly packet";
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
    const auto destAddress = core::Ipv6Address { reinterpret_cast<const std::uint8_t*>(&pPacket->destAddress()) };
    if(destAddress.isMulticast() || destAddress.toString() == _splitTunnelIp.ip6())
        return; // We drop a packet by just returnin

    // Prevent default traffic when KS=always and disconnected
    // All other traffic is fine - vpnOnly is blocked anyway and bypass is allowed
    if(_params.blockAll && !_params.isConnected)
        defaultPorts.clear();

    if(_flowTracker.track(*pPacket) == FlowTracker::RepeatedFlow)
    {
        KAPPS_CORE_INFO() << "Observed repeated packet (> 10 times), dropping" << pPacket->toString();
        return;
    }

    _defaultRuleUpdater.update(IPv6, defaultPorts, _params);
    _bypassRuleUpdater.update(IPv6, bypassPorts, _params);
    _vpnOnlyRuleUpdater.update(IPv6, vpnOnlyPorts, _params);

    // Re-inject the packet
    // Left out for now as IPv6 packet re-injection doesn't
    // work as IPv6 packets are not injected with IP headers intact
    // Instead we rely on TCP retrying the packet send after the pf rule is setup
    // UDP is not properly supported.
    // TODO: look into data link layer IPv6 injection via PF_NDRV sockets
}

void MacSplitTunnel::handleIp4(std::vector<unsigned char> buffer)
{
    // Grab size for tracing in case the packet is invalid, since we move from buffer
    auto actualSize = buffer.size();
    // skip the first 4 bytes (it stores AF_NET)
    const auto pPacket = Packet::createFromData(std::move(buffer), 4);
    if(!pPacket)
    {
        KAPPS_CORE_WARNING() << "Packet is invalid; read" << actualSize << "bytes from stun";
        return;
    }

    PiaConnections piaConnections{_executableDir, _params.tunnelDeviceLocalAddress,
        _params.netScan.ipAddress()};

    // Update the cache for non-split apps, to keep track of the ports we care about
    // when generating firewall rules
    _defaultAppsCache.refresh(IPv4, _params.netScan);

    // Get ports for our tracked apps
    auto bypassPorts = PortFinder::ports(_excludedApps, IPv4, _params.netScan);
    auto vpnOnlyPorts = PortFinder::ports(_vpnOnlyApps, IPv4, _params.netScan);
    auto defaultPorts = _defaultAppsCache.ports(IPv4);

    // These packets seem to have protocol 255, so drop them
    if(pPacket->packetType() == Packet::Other)
        return;

    // Update with our pia-specific connections
    const auto &piaBypassPorts{piaConnections.bypassPorts()};
    const auto &piaVpnOnlyPorts{piaConnections.vpnOnlyPorts()};
    bypassPorts.insert(piaBypassPorts.begin(), piaBypassPorts.end());
    vpnOnlyPorts.insert(piaVpnOnlyPorts.begin(), piaVpnOnlyPorts.end());

    // Drop vpnOnly packets when not connected
    if(!piaConnections.isPiaPort(pPacket->sourcePort()) && !_params.isConnected && pPacket->sourcePort() && contains(vpnOnlyPorts, pPacket->sourcePort()))
    {
        pid_t pid = PortFinder::pidForPort(pPacket->sourcePort(), IPv4);
        KAPPS_CORE_INFO() << "Dropping an Ipv4 vpnOnly packet " << pPacket->toString()
          << "for pid" << pid << "and path" << PortFinder::pidToPath(pid);

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
    const auto destAddress = core::Ipv4Address { pPacket->destAddress() };
    if(destAddress.isMulticast() || destAddress.isBroadcast() || (destAddress.toString() == _splitTunnelIp.ip4()))
        return; // We drop a packet by just returning

    if(_flowTracker.track(*pPacket) == FlowTracker::RepeatedFlow)
    {
        KAPPS_CORE_INFO() << "Observed repeated packet (> 10 times), dropping" << pPacket->toString();
        return;
    }

    // Prevent default traffic when KS=always and disconnected
    // All other traffic is fine - vpnOnly is blocked anyway and bypass is allowed
    if(_params.blockAll && !_params.isConnected)
    {
        defaultPorts.clear();
    }

    _defaultRuleUpdater.update(IPv4, defaultPorts, _params);
    _bypassRuleUpdater.update(IPv4, bypassPorts, _params);
    _vpnOnlyRuleUpdater.update(IPv4, vpnOnlyPorts, _params);

    //KAPPS_CORE_INFO() << "Re-injecting IPv4 packet:" << pPacket->toString();

    // Re-inject the packet
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(pPacket->destAddress());

    if(::sendto(_rawFd4->get(), pPacket->toRaw(), pPacket->len() , 0, reinterpret_cast<sockaddr *>(&to), sizeof(to)) == -1)
    {
        KAPPS_CORE_WARNING() << "Unable to reinject packet" << pPacket->toString() << "-"
            << kapps::core::ErrnoTracer{errno};
    }
}

void MacSplitTunnel::handleTunnelPacket(std::vector<unsigned char> buffer)
{
    // First 4 bytes indicate address family (IPv4 or IPv6)
    if(buffer.size() < sizeof(std::uint32_t))
    {
        KAPPS_CORE_WARNING() << "Packet doesn't have address family; size was"
            << buffer.size();
        return;
    }
    std::uint32_t addressFamily = ntohl(*reinterpret_cast<std::uint32_t *>(buffer.data()));

    switch(addressFamily)
    {
    case AF_INET:
        handleIp4(std::move(buffer));
        break;
    case AF_INET6:
        handleIp6(std::move(buffer));
        break;
    default:
        KAPPS_CORE_WARNING() << "Unsupported address family:" << addressFamily;
    }
}

}}
