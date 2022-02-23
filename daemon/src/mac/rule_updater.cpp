// Copyright (c) 2022 Private Internet Access, Inc.
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
#include "mac_splittunnel.h"
#include "ipaddress.h"

namespace
{
    const QString kDefaultApps4 = "450.routeDefaultApps4";
    const QString kBypassApps4 =  "460.routeBypassApps4";
    const QString kVpnOnlyApps4 = "470.routeVpnOnlyApps4";

    const QString kDefaultApps6 = "450.routeDefaultApps6";
    const QString kBypassApps6 =  "460.routeBypassApps6";
    const QString kVpnOnlyApps6 = "470.routeVpnOnlyApps6";
}

void RuleUpdater::clearRules(IPVersion ipVersion)
{
    PFFirewall::setFilterWithRules(_strategy->anchorNameFor(ipVersion),
                                   false, {});

    _ports[ipVersion].clear();
}

void RuleUpdater::clearAllRules()
{
    clearRules(IPv4);
    clearRules(IPv6);
}

void RuleUpdater::forceUpdate(IPVersion ipVersion, const PortSet &ports) const
{
    PFFirewall::setFilterWithRules(_strategy->anchorNameFor(ipVersion),
                                   true, {_strategy->rules(ipVersion, ports) + _strategy->routingRule(ipVersion)});
}

void RuleUpdater::update(IPVersion ipVersion, const PortSet &ports)
{
    const auto &params{_pMacSplitTunnel->params()};
    // Update the rules if the ports change OR the network has changed
    // (since our rules make use of interface and gateway ips)
    if(ports != _ports[ipVersion] || params.netScan != _netScan ||
       params.bypassIpv4Subnets != _bypassIpv4Subnets ||
       params.bypassIpv6Subnets != _bypassIpv6Subnets)
    {
        forceUpdate(ipVersion, ports);

        _ports[ipVersion] = ports;
        _netScan = params.netScan;
        _bypassIpv4Subnets = params.bypassIpv4Subnets;
        _bypassIpv6Subnets = params.bypassIpv6Subnets;
    }
}

QStringList UpdateStrategy::rules(IPVersion ipVersion, const PortSet &ports) const
{
    QStringList ruleList;

    // Tag all packets appropriately for the type of connection, e.g bypass, vpnOnly or default
    // This is used by the relevant update strategy which passes the appropriate tag type
    for(const auto &port : ports)
    {
        ruleList << QStringLiteral("pass out %1 proto {udp,tcp} from port %2 flags any no state tag %3")
            .arg(protocolFor(ipVersion))
            .arg(port)
            .arg(tagNameFor(ipVersion));
    }

    return ruleList;
}

const FirewallParams& UpdateStrategy::params() const
{
    return _pMacSplitTunnel->params();
}

const QString& UpdateStrategy::tunnelDeviceName() const
{
    return _pMacSplitTunnel->tunnelDeviceName();
}

QString BypassStrategy::tagNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? QStringLiteral("BYPASS4") : QStringLiteral("BYPASS6");
}
QString BypassStrategy::anchorNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? kBypassApps4 : kBypassApps6;
}

QStringList BypassStrategy::rules(IPVersion ipVersion, const PortSet &ports) const
{
    return UpdateStrategy::rules(ipVersion, ports);
}

QStringList BypassStrategy::routingRule(IPVersion ipVersion) const
{
    const auto &netScan = params().netScan;

    // Route all bypass apps out the physical interface
    if(!netScan.interfaceName().isEmpty() && !gatewayIp(ipVersion).isEmpty())
    {
        return QStringList{QStringLiteral("pass out route-to (%1 %2) flags any no state tagged %3")
            .arg(netScan.interfaceName(), gatewayIp(ipVersion), tagNameFor(ipVersion))};
    }
    else
    {
        qInfo() << "Can't create bypass route-to rule for"
                << ipToString(ipVersion) << "- interface or gateway not known";
        return {};
    }
}

QString VpnOnlyStrategy::tagNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? QStringLiteral("VPNONLY4") : QStringLiteral("VPNONLY6");
}

QString VpnOnlyStrategy::anchorNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? kVpnOnlyApps4 : kVpnOnlyApps6;
}

QStringList VpnOnlyStrategy::routingRule(IPVersion ipVersion) const
{
    // If tunnel is up - route all vpn-only apps out the tunnel
    if(!tunnelDeviceName().isEmpty())
        return QStringList{QStringLiteral("pass out route-to %2 flags any no state tagged %1").arg(tagNameFor(ipVersion), tunnelDeviceName())};
    else
    {
        qInfo() << "Can't create VPN-only route-to rule for" << ipToString(ipVersion) << "- tunnel interface not known";
        return {};
    }
}

QString DefaultStrategy::tagNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? QStringLiteral("DEFAULT4") : QStringLiteral("DEFAULT6");
}
QString DefaultStrategy::anchorNameFor(IPVersion ipVersion) const
{
    return ipVersion == IPv4 ? kDefaultApps4 : kDefaultApps6;
}

QStringList DefaultStrategy::routingRule(IPVersion ipVersion) const
{
    QStringList ruleList;

    // Send all default traffic out the tunnel interface if;
    // - All Other apps == Use VPN
    // - We actually have a tunnel interface up (i.e we're connected)
    if(!params().bypassDefaultApps && !tunnelDeviceName().isEmpty())
        ruleList << QStringLiteral("pass out route-to %1 flags any no state tagged %2").arg(tunnelDeviceName(), tagNameFor(ipVersion));

   // Otherwise send all default traffic out the physical interface
    else
    {
        const auto &netScan = params().netScan;
        if(!netScan.interfaceName().isEmpty() && !gatewayIp(ipVersion).isEmpty())
            ruleList << QStringLiteral("pass out route-to (%1 %2) flags any no state tagged %3").arg(netScan.interfaceName(), gatewayIp(ipVersion), tagNameFor(ipVersion));
        else
            qInfo() << "Can't create default route-to rule for" << ipToString(ipVersion) << "- interface or gateway not known" << netScan;
    }

    return ruleList;
}

QStringList DefaultStrategy::lanSubnetsFor(IPVersion ipVersion) const
{
    if(ipVersion == IPv4)
        return QStringList{"127.0.0.0/8", "10.0.0.0/8", "169.254.0.0/16", "172.16.0.0/12", "192.168.0.0/16", "224.0.0.0/4", "255.255.255.255/32"};
    else
    {
        QStringList ruleList{"fc00::/7", "fe80::/10", "ff00::/8"};
        // If ipv6 is available ensure we also include the ipv6prefix subnet
        if(params().netScan.hasIpv6())
            ruleList << QStringLiteral("%1/64").arg(params().netScan.ipAddress6());

        return ruleList;
    }
}

QStringList DefaultStrategy::bypassSubnetsFor(IPVersion ipVersion) const
{
    if(ipVersion == IPv4)
        return params().bypassIpv4Subnets.toList();
    else
        return params().bypassIpv6Subnets.toList();
}

QStringList DefaultStrategy::rules(IPVersion ipVersion, const PortSet &ports) const
{
    // Add LAN ips to the lanips table. Each table is unique to each anchor - so lanips in kDefaultApps4 is
    // different to the lanips table in kDefaultApps6.
    // We need to use a table to store the ips so that the !<lanips> rule will work - it's impossible to
    // otherwise negate a specific list of ips for use in a rule.
    // Also since bypass subnets are handled by their own specific routes, we don't want to re-route icmp bound for those.
    PFFirewall::setAnchorTable(anchorNameFor(ipVersion), true, QStringLiteral("lanips"), lanSubnetsFor(ipVersion) + bypassSubnetsFor(ipVersion));

    QStringList ruleList{UpdateStrategy::rules(ipVersion, ports)};

    // Send all ICMP (that're not headed towards LAN or bypass ips) out the default interface - we're essentially
    // saying: "manage icmp for all ips EXCEPT LAN and bypass ips"
    // LAN and bypass ips are instead handled by the "allowLAN" and "allowSubnets" rules defined in the main ruleset.
    if(!params().blockAll || params().isConnected)
        ruleList << QStringLiteral("pass out %1 proto %2 to !<lanips> flags any no state tag %3").arg(protocolFor(ipVersion)).arg(icmpVersion(ipVersion)).arg(tagNameFor(ipVersion));

    return ruleList;
}
