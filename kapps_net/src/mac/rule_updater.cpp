// Copyright (c) 2024 Private Internet Access, Inc.
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


#include "rule_updater.h"
#include "pf_firewall.h"
#include <kapps_core/src/ipaddress.h>
#include <kapps_core/src/logger.h>

namespace
{
    const std::string kDefaultApps4 = "450.routeDefaultApps4";
    const std::string kBypassApps4 =  "460.routeBypassApps4";
    const std::string kVpnOnlyApps4 = "470.routeVpnOnlyApps4";

    const std::string kDefaultApps6 = "450.routeDefaultApps6";
    const std::string kBypassApps6 =  "460.routeBypassApps6";
    const std::string kVpnOnlyApps6 = "470.routeVpnOnlyApps6";
}
namespace
{
    using StringVector = UpdateStrategy::StringVector;
}

void RuleUpdater::clearRules(IPVersion ipVersion)
{
    _filter.setFilterWithRules(_strategy->anchorNameFor(ipVersion), false, {});
    _ports[ipVersion].clear();
}

void RuleUpdater::clearAllRules()
{
    clearRules(IPv4);
    clearRules(IPv6);
}

void RuleUpdater::forceUpdate(IPVersion ipVersion, const PortSet &ports,
                              const kapps::net::FirewallParams &params) const
{
    StringVector vec;

    const auto &rules = _strategy->rules(ipVersion, ports, params);
    const auto &routingRules = _strategy->routingRule(ipVersion, params);

    vec.insert(vec.end(), rules.begin(), rules.end());
    vec.insert(vec.end(), routingRules.begin(), routingRules.end());

    _filter.setFilterWithRules(_strategy->anchorNameFor(ipVersion), true, vec);
}

void RuleUpdater::update(IPVersion ipVersion, const PortSet &ports,
                         const kapps::net::FirewallParams &params)
{
    // Update the rules if the ports change OR the network has changed
    // (since our rules make use of interface and gateway ips)
    if(ports != _ports[ipVersion] || params.netScan != _netScan ||
       params.bypassIpv4Subnets != _bypassIpv4Subnets ||
       params.bypassIpv6Subnets != _bypassIpv6Subnets)
    {
        forceUpdate(ipVersion, ports, params);

        _ports[ipVersion] = ports;
        _netScan = params.netScan;
        _bypassIpv4Subnets = params.bypassIpv4Subnets;
        _bypassIpv6Subnets = params.bypassIpv6Subnets;
    }
}

StringVector UpdateStrategy::rules(IPVersion ipVersion, const PortSet &ports,
                                   const kapps::net::FirewallParams &) const
{
    std::vector<std::string> ruleList;

    // Tag all packets appropriately for the type of connection, e.g bypass, vpnOnly or default
    // This is used by the relevant update strategy which passes the appropriate tag type
    for(const auto &port : ports)
    {
        ruleList.push_back(qs::format("pass out % proto {udp,tcp} from port % flags any no state tag %",
            protocolFor(ipVersion), port, tagNameFor(ipVersion)));
    }

    return ruleList;
}

kapps::core::StringSlice BypassStrategy::tagNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? "BYPASS4" : "BYPASS6";
}

kapps::core::StringSlice BypassStrategy::anchorNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? kBypassApps4 : kBypassApps6;
}

StringVector BypassStrategy::routingRule(IPVersion ipVersion,
                                         const kapps::net::FirewallParams &params) const
{
    const auto &netScan = params.netScan;

    // Route all bypass apps out the physical interface
    if(!netScan.interfaceName().empty() && !gatewayIp(ipVersion, params).empty())
    {
        return StringVector{qs::format("pass out route-to (% %) flags any no state tagged %",
            netScan.interfaceName(), gatewayIp(ipVersion, params), tagNameFor(ipVersion))};
    }
    else
    {
        KAPPS_CORE_INFO() << "Can't create bypass route-to rule for"
                << ipToString(ipVersion) << "- interface or gateway not known";
        return {};
    }
}

kapps::core::StringSlice VpnOnlyStrategy::tagNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? "VPNONLY4" : "VPNONLY6";
}

kapps::core::StringSlice VpnOnlyStrategy::anchorNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? kVpnOnlyApps4 : kVpnOnlyApps6;
}

StringVector VpnOnlyStrategy::routingRule(IPVersion ipVersion,
                                          const kapps::net::FirewallParams &params) const
{
    // If tunnel is up - route all vpn-only apps out the tunnel
    if(!params.tunnelDeviceName.empty())
    {
        return StringVector{qs::format("pass out route-to % flags any no state tagged %",
                                       params.tunnelDeviceName, tagNameFor(ipVersion))};
    }
    else
    {
        KAPPS_CORE_INFO() << "Can't create VPN-only route-to rule for" << ipToString(ipVersion)
            << "- tunnel interface not known";
        return {};
    }
}

kapps::core::StringSlice DefaultStrategy::tagNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? "DEFAULT4" : "DEFAULT6";
}
kapps::core::StringSlice DefaultStrategy::anchorNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? kDefaultApps4 : kDefaultApps6;
}

StringVector DefaultStrategy::routingRule(IPVersion ipVersion,
                                          const kapps::net::FirewallParams &params) const
{
    StringVector ruleList;

    // Send all default traffic out the tunnel interface if;
    // - All Other apps == Use VPN
    // - We actually have a tunnel interface up (i.e we're connected)
    if(!params.bypassDefaultApps && !params.tunnelDeviceName.empty())
    {
        ruleList.push_back(qs::format("pass out route-to % flags any no state tagged %",
                           params.tunnelDeviceName, tagNameFor(ipVersion)));
    }

    // Otherwise send all default traffic out the physical interface
    else
    {
        const auto &netScan = params.netScan;
        if(!netScan.interfaceName().empty() && !gatewayIp(ipVersion, params).empty())
        {
            ruleList.push_back(qs::format("pass out route-to (% %) flags any no state tagged %",
                                          netScan.interfaceName(), gatewayIp(ipVersion, params),
                                          tagNameFor(ipVersion)));
        }
        else
        {
            KAPPS_CORE_INFO() << "Can't create default route-to rule for"
                << ipToString(ipVersion) << "- interface or gateway not known"
                << netScan;
        }
    }

    return ruleList;
}

std::set<std::string> DefaultStrategy::lanSubnetsFor(IPVersion ipVersion,
                                                     const kapps::net::FirewallParams &params) const
{
    if(ipVersion == IPv4)
    {
        return std::set<std::string>{"127.0.0.0/8", "10.0.0.0/8", "169.254.0.0/16",
                                     "172.16.0.0/12", "192.168.0.0/16", "224.0.0.0/4",
                                     "255.255.255.255/32"};
    }
    else
    {
        std::set<std::string> ips{"fc00::/7", "fe80::/10", "ff00::/8"};
        // If ipv6 is available ensure we also include the ipv6prefix subnet
        if(params.netScan.hasIpv6())
            ips.insert(qs::format("%/64", params.netScan.ipAddress6()));

        return ips;
    }
}

std::set<std::string> DefaultStrategy::bypassSubnetsFor(IPVersion ipVersion,
                                                        const kapps::net::FirewallParams &params) const
{
    if(ipVersion == IPv4)
        return params.bypassIpv4Subnets;
    else
        return params.bypassIpv6Subnets;
}

StringVector DefaultStrategy::rules(IPVersion ipVersion, const PortSet &ports,
                                    const kapps::net::FirewallParams &params) const
{
    // Add LAN ips to the lanips table. Each table is unique to each anchor - so lanips in kDefaultApps4 is
    // different to the lanips table in kDefaultApps6.
    // We need to use a table to store the ips so that the !<lanips> rule will work - it's impossible to
    // otherwise negate a specific list of ips for use in a rule.
    // Also since bypass subnets are handled by their own specific routes, we don't want to re-route icmp bound for those.
    const auto &lanSubnets = lanSubnetsFor(ipVersion, params);
    const auto &bypassSubnets = bypassSubnetsFor(ipVersion, params);
    StringVector vec;
    vec.insert(vec.end(), lanSubnets.begin(), lanSubnets.end());
    vec.insert(vec.end(), bypassSubnets.begin(), bypassSubnets.end());

    _filter.setAnchorTable(anchorNameFor(ipVersion), true, "lanips", vec);

    StringVector ruleList{UpdateStrategy::rules(ipVersion, ports, params)};

    // Send all ICMP (that're not headed towards LAN or bypass ips) out the default interface - we're essentially
    // saying: "manage icmp for all ips EXCEPT LAN and bypass ips"
    // LAN and bypass ips are instead handled by the "allowLAN" and "allowSubnets" rules defined in the main ruleset.
    if(!params.blockAll || params.isConnected)
    {
        ruleList.push_back(qs::format("pass out % proto % to !<lanips> flags any no state tag %",
                                      protocolFor(ipVersion), icmpVersion(ipVersion),
                                      tagNameFor(ipVersion)));
    }

    return ruleList;
}
