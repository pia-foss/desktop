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


#include "rule_updater.h"
#include "mac_splittunnel.h"

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
    const auto params{_pMacSplitTunnel->params()};
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
    for(const auto &port : ports)
        ruleList << QStringLiteral("pass out %1 proto {udp,tcp} from port %2 flags any no state tag %3")
            .arg(protocolFor(ipVersion))
            .arg(port)
            .arg(tagNameFor(ipVersion));

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
    return UpdateStrategy::rules(ipVersion, ports) +
        bypassSubnetRules(ipVersion);
}

QStringList BypassStrategy::bypassSubnetRules(IPVersion ipVersion) const
{
    QStringList ruleList;
    // No need for ipv6 support as we don't support it yet while connected
    if(ipVersion == IPv4)
    {
        auto ip4Subnets = QStringList{params().bypassIpv4Subnets.toList()};
        PFFirewall::setAnchorTable(kBypassApps4, true, QStringLiteral("subnets"), ip4Subnets);
        ruleList << QStringLiteral("pass out to <subnets> flags any no state tag %1").arg(tagNameFor(ipVersion));
    }

    return ruleList;
}

QStringList BypassStrategy::routingRule(IPVersion ipVersion) const
{
    const auto &netScan = params().netScan;

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

    if(!params().blockAll || params().isConnected)
    {
       ruleList << QStringLiteral("pass out %1 proto %2 all flags any no state tag %3").arg(protocolFor(ipVersion)).arg(icmpVersion(ipVersion)).arg(tagNameFor(ipVersion));
    }

    if(!params().bypassDefaultApps && !tunnelDeviceName().isEmpty())
        ruleList << QStringLiteral("pass out route-to %1 flags any no state tagged %2").arg(tunnelDeviceName(), tagNameFor(ipVersion));
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

QStringList DefaultStrategy::rules(IPVersion ipVersion, const PortSet &ports) const
{
    static const QStringList icmp4Exceptions{"pass out quick proto icmp to { 10.0.0.0/8, 169.254.0.0/16, 172.16.0.0/12, 192.168.0.0/16, 224.0.0.0/4, 255.255.255.255/32 } no state"};
    static const QStringList icmp6Exceptions{"pass out quick proto icmp6 to { fc00::/7, fe80::/10, ff00::/8 } no state"};

    return UpdateStrategy::rules(ipVersion, ports) + icmp4Exceptions + icmp6Exceptions;
}
