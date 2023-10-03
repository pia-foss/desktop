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

#include "mac_firewall.h"
#include <thread>
#include <sstream>
#include <tuple>
#include <regex>
#include <kapps_core/src/logger.h>
#include <kapps_core/src/ipaddress.h>
#include <kapps_core/src/newexec.h>
#include <kapps_core/src/mac/mac_version.h>
#include <kapps_core/src/configwriter.h>

namespace
{
    // std::tuple does a lexicographical comparison which
    // is just what we want.
    using Version = std::tuple<unsigned, unsigned, unsigned>;

     // Restart strategy for stub resolver
    const kapps::core::RestartStrategy::Params stubResolverRestart{std::chrono::milliseconds(100), // Min restart delay
                                                      std::chrono::seconds(3), // Max restart delay
                                                      std::chrono::seconds(30)}; // Min "successful" run time
}

namespace kapps { namespace net {

StubUnbound::StubUnbound(FirewallConfig config)
    : _unboundRunner{stubResolverRestart}, _config{std::move(config)}
{
}

bool StubUnbound::enable(bool enabled)
{
    if(enabled == _unboundRunner.isEnabled())
        return false; // No change, nothing to do

    if(enabled)
    {
        KAPPS_CORE_INFO() << "Initialize DNS stub";

        {
            kapps::core::ConfigWriter conf{_config.unboundDnsStubConfigFile};
            conf << "server:" << conf.endl;
            conf << "    logfile: \"\"" << conf.endl;   // Log to stderr
            conf << "    edns-buffer-size: 4096" << conf.endl;
            conf << "    max-udp-size: 4096" << conf.endl;
            conf << "    qname-minimisation: yes" << conf.endl;
            conf << "    interface: 127.0.0.1@8073" << conf.endl;
            conf << "    interface: ::1@8073" << conf.endl;
            // This server shouldn't do any queries, bind it to the loopback
            // interface
            conf << "    outgoing-interface: 127.0.0.1" << conf.endl;
            conf << "    verbosity: 1" << conf.endl;
            // We can drop user/group rights on this server because it doesn't
            // have to make any queries
            conf << "    username: \"nobody\"" << conf.endl;
            conf << "    do-daemonize: no" << conf.endl;
            conf << "    use-syslog: no" << conf.endl;
            conf << "    hide-identity: yes" << conf.endl;
            conf << "    hide-version: yes" << conf.endl;
            // By default, unbound only allows queries from localhost.  Due to
            // the PF redirections used to redirect outgoing queries to this
            // server, it will receive packets on the loopback interface with
            // the source IPs of other interfaces, so we need to allow any host.
            // This doesn't open up access to remote hosts.
            conf << "    access-control: 0.0.0.0/0 allow" << conf.endl;
            conf << "    access-control: ::/0 allow" << conf.endl;
            conf << "    directory: \"" << _config.installationDir << "\"" << conf.endl;
            conf << "    pidfile: \"\"" << conf.endl;
            conf << "    chroot: \"\"" << conf.endl;

            // This causes an NXDOMAIN response for all domains.
            conf << "    local-zone: \".\" static" << conf.endl;
        }
        _unboundRunner.enable(_config.unboundExecutableFile,
                              {"-c", _config.unboundDnsStubConfigFile});
        // Don't need to flush the DNS cache when enabling the stub resolver
        return false;
    }
    else
    {
        _unboundRunner.disable();
        core::removeFile(_config.unboundDnsStubConfigFile);
        // Flush the DNS cache in case the bogus responses were cached
        return true;
    }
}

StubUnbound::~StubUnbound()
{
    enable(false);  // Remove the config file
}

class MacRouteManager : public RouteManager
{
public:
    virtual void addRoute4(const std::string &subnet, const std::string &gatewayIp, const  std::string &interfaceName, uint32_t metric=0) const override;
    virtual void removeRoute4(const std::string &subnet, const  std::string &gatewayIp, const  std::string &interfaceName) const override;
    virtual void addRoute6(const std::string &subnet, const  std::string &gatewayIp, const  std::string &interfaceName, uint32_t metric=0) const override;
    virtual void removeRoute6(const std::string &subnet, const  std::string &gatewayIp, const  std::string &interfaceName) const override;
};

MacFirewall::MacFirewall(FirewallConfig config)
    : _config{std::move(config)}, _stubUnbound{_config},
      _subnetBypass{std::make_unique<MacRouteManager>()},
      _executableDir{_config.executableDir}
{
    _pFilter.emplace(_config);
    _pFilter->install();
}

MacFirewall::~MacFirewall()
{
    assert(_pFilter);   // Class invariant
    _pFilter->uninstall();
    KAPPS_CORE_INFO() << "Mac firewall shutdown complete";
}

void MacFirewall::applyRules(const FirewallParams &params)
{
    assert(_pFilter);   // Class invariant

    if(!_pFilter->isInstalled())
    {
        KAPPS_CORE_INFO() << "Firewall not installed, installing..";
        _pFilter->install();
    }

    const auto netScan{params.netScan};

    _pFilter->ensureRootAnchorPriority();

    _pFilter->setTranslationEnabled("000.natVPN", params.hasConnected, { {"interface", params.tunnelDeviceName} });
    _pFilter->setFilterWithRules("001.natPhys", params.enableSplitTunnel, natPhysRules(netScan));
    _pFilter->setFilterEnabled("000.allowLoopback", params.allowLoopback);
    _pFilter->setFilterEnabled("100.blockAll", params.blockAll);
    _pFilter->setFilterEnabled("200.allowVPN", params.allowVPN, { {"interface", params.tunnelDeviceName} });
    _pFilter->setFilterEnabled("250.blockIPv6", params.blockIPv6);

    _pFilter->setFilterEnabled("290.allowDHCP", params.allowDHCP);
    _pFilter->setFilterEnabled("299.allowIPv6Prefix", netScan.hasIpv6() && params.allowLAN);
    _pFilter->setAnchorTable("299.allowIPv6Prefix", netScan.hasIpv6() && params.allowLAN, "ipv6prefix", {
        // First 64 bits is the IPv6 Network Prefix
        qs::format("%/64", netScan.ipAddress6())});
    _pFilter->setFilterEnabled("305.allowSubnets", params.enableSplitTunnel);
    _pFilter->setAnchorTable("305.allowSubnets", params.enableSplitTunnel, "subnets", subnetsToBypass(params));
    _pFilter->setFilterEnabled("490.allowLAN", params.allowLAN);
    // Get data for setting up our DNS leak protection rules
    const auto dnsLeakProtection = getDnsLeakProtectionInfo(params);
    _pFilter->setTranslationEnabled("000.stubDNS", dnsLeakProtection.macStubDNS);
    if(_stubUnbound.enable(dnsLeakProtection.macStubDNS))
    {
        // Schedule a DNS cache flush since the DNS stub was disabled; important
        // if the user disables the VPN while in this state.
        dnsCacheFlush();
    }

    _pFilter->setFilterEnabled("400.allowPIA", params.allowPIA);
    _pFilter->setFilterEnabled("500.blockDNS", dnsLeakProtection.macBlockDNS, { {"interface", params.tunnelDeviceName} });
    _pFilter->setAnchorTable("500.blockDNS", dnsLeakProtection.macBlockDNS, "localdns", dnsLeakProtection.localDnsServers);
    _pFilter->setAnchorTable("500.blockDNS", dnsLeakProtection.macBlockDNS, "tunneldns", dnsLeakProtection.tunnelDnsServers);
    _pFilter->setFilterEnabled("510.stubDNS", dnsLeakProtection.macStubDNS);
    _pFilter->setFilterEnabled("520.allowHnsd", params.allowResolver, { { "interface", params.tunnelDeviceName } });

    _subnetBypass.updateRoutes(params);
    _boundRouteUpdater.update(params);
    toggleSplitTunnel(params);
}

void MacFirewall::aboutToConnectToVpn()
{
    if(!_pSplitTunnel)
        return;
    assert(_pSplitTunnelWorker);   // Class invariant - exists only when _pSplitTunnel exists

    // Cycle the split tunnel device synchronously.  This needs to happen before
    // the VPN connection occurs.
    _pSplitTunnelWorker->syncInvoke([&]()
        {
            KAPPS_CORE_INFO() << "Notifying split tunnel that we're about to connect the VPN";

            // Cycle the split tunnel device
            _pSplitTunnel->aboutToConnectToVpn();
        });
}

void MacFirewall::startSplitTunnel(const FirewallParams &params)
{
    if(_pSplitTunnel)
        return;
    assert(!_pSplitTunnelWorker);   // Class invariant - exists only when _pSplitTunnel exists

    // Create a worker thread to operate the split tunnel device from userspace.
    // We don't use any work items (we use syncInvoke() to update firewall
    // params and signal the "about-to-connect" condition), so the handler is
    // empty.
    _pSplitTunnelWorker.emplace([](core::Any){});

    // Start the split tunnel implementation on the worker thread.
    _pSplitTunnelWorker->syncInvoke([&]
    {
        _pSplitTunnel.emplace(params, _executableDir, *_pFilter);
    });
}

void MacFirewall::stopSplitTunnel()
{
    // Shut down the worker thread
    _pSplitTunnelWorker.clear();
    // Then, shut down the utun device
    _pSplitTunnel.clear();
}

void MacFirewall::updateSplitTunnel(const FirewallParams &params)
{
    if(!_pSplitTunnel)
        return;
    assert(_pSplitTunnelWorker);   // Class invariant - exists only when _pSplitTunnel exists

    // Reconfigure the firewall synchronously on the worker thread.
    // Alternatively, we could copy the params and then queue it to the worker
    // thread, but the worker shouldn't block us for long, and we should not
    // risk proceeding with a VPN connection/disconnection (etc.) if the split
    // tunnel rules have changed (don't risk briefly leaking an app while the
    // update is queued.)
    _pSplitTunnelWorker->syncInvoke([&]()
        {
            KAPPS_CORE_INFO() << "Updating split tunnel configuration";
            _pSplitTunnel->updateSplitTunnel(params);
        });
}

std::vector<std::string> MacFirewall::natPhysRules(const OriginalNetworkScan &netScan)
{
    // Mojave (10.14.* and below) kernel panics when we enable nat for ipv6
    // so the first version safe for nat6 is 10.15.0
    const Version safeNat6Version{10, 15, 0};
    const Version currentVersion{core::currentMacosVersion()};

    const std::string itfName = netScan.interfaceName();
    std::string inetLanIps{"{ 10.0.0.0/8, 169.254.0.0/16, 172.16.0.0/12, 192.168.0.0/16, 224.0.0.0/4, 255.255.255.255/32 }"};
    std::string inet6LanIps{"{ fc00::/7, fe80::/10, ff00::/8 }"};
    std::vector<std::string> ruleList{qs::format("no nat on % inet6 from any to %", itfName, inet6LanIps),
                         qs::format("no nat on % inet from any to %", itfName, inetLanIps),
                         qs::format("nat on % inet -> (%)", itfName, itfName)};

    if(currentVersion >= safeNat6Version)
        ruleList.push_back(qs::format("nat on % inet6 -> (%)", itfName, itfName));
    else
        KAPPS_CORE_INFO() << qs::format("Not creating inet6 nat rule for % - current mac version is < 10.15.0", itfName);

    return ruleList;
}

std::vector<std::string> MacFirewall::subnetsToBypass(const FirewallParams &params)
{
    std::vector<std::string> bypassIpv4Subnets{params.bypassIpv4Subnets.begin(), params.bypassIpv4Subnets.end()};
    std::vector<std::string> bypassIpv6Subnets{params.bypassIpv6Subnets.begin(), params.bypassIpv6Subnets.end()};

    if(bypassIpv6Subnets.empty())
    {
        // No IPv6 subnets, so just return IPv4
        return bypassIpv4Subnets;
    }
    else
    {
        // If we have any IPv6 subnets then Whitelist link-local/broadcast IPv6 ranges too.
        // These are required by IPv6 Neighbor Discovery
        std::vector<std::string> ipv6Base{"fe80::/10", "ff00::/8"};
        return qs::concat(ipv6Base, bypassIpv4Subnets, bypassIpv6Subnets);
    }
}

MacFirewall::DNSLeakProtectionInfo MacFirewall::getDnsLeakProtectionInfo(const FirewallParams &params)
{
    // On Mac, there are two DNS leak protection modes depending on whether we
    // are connected.
    //
    // These modes are needed to handle quirks in mDNSResponder.  It has been
    // observed sending DNS packets on the physical interface even when the
    // DNS server is properly routed via the VPN.  When resuming from sleep, it
    // also may prevent traffic from being sent over the physical interface
    // until it has received DNS responses over that interface (which we don't
    // want to allow to prevent leaks).
    //
    // - When connected, 310.blockDNS blocks access to UDP/TCP 53 on servers
    //   other than the configured DNS servers, and UDP/TCP 53 to the configured
    //   servers is forced onto the tunnel, even if the sender had bound to the
    //   physical interface.
    // - In any other state, 000.stubDNS redirects all UDP/TCP 53 to a local
    //   resolver that just returns NXDOMAIN for all queries.  This should
    //   satisfy mDNSResponder without creating leaks.
    DNSLeakProtectionInfo info{};

    // In addition to the normal case (connected) some versions of macOS
    // set the PrimaryService Key to empty when switching networks.
    // In this case we want to use stubDNS to work-around this behavior.
    if(!params.tunnelDeviceName.empty() &&
       !params.macosPrimaryServiceKey.empty())
    {
        info.macBlockDNS = params.blockDNS;
        for(const auto &address : params.effectiveDnsServers)
        {
            core::Ipv4Address parsed{address};
            if(!parsed.isLocalDNS())
                info.tunnelDnsServers.push_back(address);
            else
                info.localDnsServers.push_back(address);
        }
    }
    else
    {
        info.macStubDNS = params.blockDNS;
    }

    return info;
}

// TODO: Should this function bellong to this class?
void MacFirewall::dnsCacheFlush() const
{
    std::thread([]
    {
        core::Exec::cmd("dscacheutil", {"-flushcache"});
        core::Exec::cmd("discoveryutil", {"udnsflushcaches"});
        core::Exec::cmd("discoveryutil", {"mdnsflushcache"});
        core::Exec::cmd("killall", {"-HUP", "mDNSResponder"});
        core::Exec::cmd("killall", {"-HUP", "mDNSResponderHelper"});
    }).detach();
}

void BoundRouteUpdater::update(const FirewallParams &params)
{
    // We may need a bound route for the physical interface:
    // - When we have connected (even if currently reconnecting) - we need this
    //   for DNS leak protection.  Apps can try to send DNS packets out the
    //   physical interface toward the configured DNS servers, but PIA forces it
    //   it into the tunnel anyway.  (mDNSResponder does this in 10.15.4+.)
    // - Split tunnel (even if disconnected) - needed to allow bypass apps to
    //   bind to the physical interface, and needed for OpenVPN itself to bind
    //   to the physical interface to bypass split tunnel.
    if(params.enableSplitTunnel || params.hasConnected)
    {
        // Remove the previous bound route if it's present and different
        if(_netScan.ipv4Valid() && (_netScan.gatewayIp() != params.netScan.gatewayIp() ||
            _netScan.interfaceName() != params.netScan.interfaceName()))
        {
            KAPPS_CORE_INFO() << "Network has changed from" << _netScan.interfaceName() << "/"  << _netScan.gatewayIp() << "to"
                << params.netScan.interfaceName() << "/"  << params.netScan.gatewayIp() << "- create new bound route";
            removeBoundRoute(_netScan.gatewayIp(), _netScan.interfaceName());
        }

        // Add the new bound route.  Do this even if it doesn't seem to have
        // changed, because the route can be lost if the user switches to a new
        // network on the same interface with the same gateway (common with
        // 2.4GHz <-> 5GHz network switching)
        if(params.netScan.ipv4Valid())
        {
            // Trace this only when it appears to be new
            if(!_netScan.ipv4Valid())
            {
                KAPPS_CORE_INFO() << "Creating bound route for new network" << params.netScan.interfaceName() << "/"
                    << params.netScan.gatewayIp();
            }
            createBoundRoute(params.netScan.gatewayIp(), params.netScan.interfaceName());
        }

        _netScan = params.netScan;
    }
    else
    {
        // Remove the bound route if it's there
        if(_netScan.ipv4Valid())
        {
            removeBoundRoute(_netScan.gatewayIp(), _netScan.interfaceName());
        }
        _netScan = {};
    }
}

void BoundRouteUpdater::createBoundRoute(const std::string &ipAddress, const std::string &interfaceName)
{
    // Checked by caller (OriginalNetworkScan::ipv4Valid() below)
    assert(!ipAddress.empty());
    assert(!interfaceName.empty());

    kapps::core::Exec::bash(qs::format("/sbin/route add -net 0.0.0.0 % -ifscope %", ipAddress, interfaceName));
};

void BoundRouteUpdater::removeBoundRoute(const std::string &ipAddress, const std::string &interfaceName)
{
    // Checked by caller (OriginalNetworkScan::ipv4Valid() below)
    assert(!ipAddress.empty());
    assert(!interfaceName.empty());

    kapps::core::Exec::bash(qs::format("/sbin/route delete 0.0.0.0 -interface % -ifscope %", interfaceName, interfaceName));
}

void MacRouteManager::addRoute4(const std::string &subnet, const  std::string &gatewayIp, const  std::string &interfaceName, uint32_t metric) const
{
    KAPPS_CORE_INFO() << "Adding ipv4 bypass route for" << subnet;
    int result = kapps::core::Exec::cmd("/sbin/route", {"add", "-net", subnet, gatewayIp});
    KAPPS_CORE_INFO() << "route result" << result;
}

void MacRouteManager::removeRoute4(const std::string &subnet, const  std::string &gatewayIp, const  std::string &interfaceName) const
{
    KAPPS_CORE_INFO() << "Removing ipv4 bypass route for" << subnet;
    int result = kapps::core::Exec::cmd("/sbin/route", {"delete", "-net", subnet, gatewayIp});
    KAPPS_CORE_INFO() << "route result" << result;
}

// sudo route -q -n delete -inet6 2a03:b0c0:2:d0::26:c001 fe80::325a:3aff:fe6d:a1e0
void MacRouteManager::addRoute6(const std::string &subnet, const  std::string &gatewayIp, const  std::string &interfaceName, uint32_t metric) const
{
    KAPPS_CORE_INFO() << "Adding ipv6 bypass route for" << subnet;
    std::stringstream s;
    // We use a stringstream as qs::format makes it difficult due to '%' being used as a delimiter
    s << gatewayIp << "%" << interfaceName;
    kapps::core::Exec::cmd("/sbin/route", {"add", "-inet6", subnet, s.str()});
}

void MacRouteManager::removeRoute6(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName) const
{
    KAPPS_CORE_INFO() << "Removing ipv6 bypass route for" << subnet;
    std::stringstream s;
    // We use a stringstream as qs::format makes it difficult due to '%' being used as a delimiter
    s << gatewayIp << "%" << interfaceName;
    kapps::core::Exec::cmd("/sbin/route", {"delete", "-inet6", subnet, s.str()});
}

}}
