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
#line HEADER_FILE("posix/posix_firewall_iptables.cpp")

#ifdef Q_OS_LINUX

#include "posix_firewall_iptables.h"
#include "daemon.h" // FirewallParams
#include "path.h"
#include "brand.h"
#include "exec.h"
#include "linux/linux_cgroup.h"
#include "linux/linux_fwmark.h"
#include "linux/linux_routing.h"

#include <QProcess>

namespace
{
    const QString kAnchorName{BRAND_CODE "vpn"};
}

QString IpTablesFirewall::kOutputChain = QStringLiteral("OUTPUT");
QString IpTablesFirewall::kPostRoutingChain = QStringLiteral("POSTROUTING");
QString IpTablesFirewall::kPreRoutingChain = QStringLiteral("PREROUTING");
QString IpTablesFirewall::kRootChain = QStringLiteral("%1.anchors").arg(kAnchorName);
QString IpTablesFirewall::kFilterTable = QStringLiteral("filter");
QString IpTablesFirewall::kNatTable = QStringLiteral("nat");
QString IpTablesFirewall::kRawTable = QStringLiteral("raw");
QString IpTablesFirewall::kMangleTable = QStringLiteral("mangle");
QString IpTablesFirewall::kVpnGroupName = BRAND_CODE "vpn";
QString IpTablesFirewall::kHnsdGroupName = BRAND_CODE "hnsd";

static QString getCommand(IpTablesFirewall::IPVersion ip)
{
    return ip == IpTablesFirewall::IPv6 ? QStringLiteral("ip6tables") : QStringLiteral("iptables");
}

int IpTablesFirewall::createChain(IpTablesFirewall::IPVersion ip, const QString& chain, const QString& tableName)
{
    if (ip == Both)
    {
        int result4 = createChain(IPv4, chain, tableName);
        int result6 = createChain(IPv6, chain, tableName);
        return result4 ? result4 : result6;
    }
    const QString cmd = getCommand(ip);
    return execute(QStringLiteral("%1 -w -N %2 -t %3 || %1 -F %2 -t %3").arg(cmd, chain, tableName));
}

int IpTablesFirewall::deleteChain(IpTablesFirewall::IPVersion ip, const QString& chain, const QString& tableName)
{
    if (ip == Both)
    {
        int result4 = deleteChain(IPv4, chain, tableName);
        int result6 = deleteChain(IPv6, chain, tableName);
        return result4 ? result4 : result6;
    }
    const QString cmd = getCommand(ip);
    return execute(QStringLiteral("if %1 -w -L %2 -n -t %3 > /dev/null 2> /dev/null ; then %1 -w -F %2 -t %3 && %1 -X %2 -t %3; fi").arg(cmd, chain, tableName));
}

int IpTablesFirewall::linkChain(IpTablesFirewall::IPVersion ip, const QString& chain, const QString& parent, bool mustBeFirst, const QString& tableName)
{
    if (ip == Both)
    {
        int result4 = linkChain(IPv4, chain, parent, mustBeFirst, tableName);
        int result6 = linkChain(IPv6, chain, parent, mustBeFirst, tableName);
        return result4 ? result4 : result6;
    }
    const QString cmd = getCommand(ip);
    if (mustBeFirst)
    {
        // This monster shell script does the following:
        // 1. Check if a rule with the appropriate target exists at the top of the parent chain
        // 2. If not, insert a jump rule at the top of the parent chain
        // 3. Look for and delete a single rule with the designated target at an index > 1
        //    (we can't safely delete all rules at once since rule numbers change)
        // TODO: occasionally this script results in warnings in logs "Bad rule (does a matching rule exist in the chain?)" - this happens when
        // the e.g OUTPUT chain is empty but this script attempts to delete things from it anyway. It doesn't cause any problems, but we should still fix at some point..
        return execute(QStringLiteral("if ! %1 -w -L %2 -n --line-numbers -t %4 2> /dev/null | awk 'int($1) == 1 && $2 == \"%3\" { found=1 } END { if(found==1) { exit 0 } else { exit 1 } }' ; then %1 -w -I %2 -j %3 -t %4 && %1 -L %2 -n --line-numbers -t %4 2> /dev/null | awk 'int($1) > 1 && $2 == \"%3\" { print $1; exit }' | xargs %1 -w -t %4 -D %2 ; fi").arg(cmd, parent, chain, tableName));
    }
    else
        return execute(QStringLiteral("if ! %1 -w -C %2 -j %3 -t %4 2> /dev/null ; then %1 -w -A %2 -j %3 -t %4; fi").arg(cmd, parent, chain, tableName));
}

int IpTablesFirewall::unlinkChain(IpTablesFirewall::IPVersion ip, const QString& chain, const QString& parent, const QString& tableName)
{
    if (ip == Both)
    {
        int result4 = unlinkChain(IPv4, chain, parent, tableName);
        int result6 = unlinkChain(IPv6, chain, parent, tableName);
        return result4 ? result4 : result6;
    }
    const QString cmd = getCommand(ip);
    return execute(QStringLiteral("if %1 -w -C %2 -j %3 -t %4 2> /dev/null ; then %1 -w -D %2 -j %3 -t %4; fi").arg(cmd, parent, chain, tableName));
}

void IpTablesFirewall::ensureRootAnchorPriority(IpTablesFirewall::IPVersion ip)
{
    linkChain(ip, kRootChain, kOutputChain, true);
}

void IpTablesFirewall::installAnchor(IpTablesFirewall::IPVersion ip, const QString& anchor, const QStringList& rules, const QString& tableName)

{

    if (ip == Both)
    {
        installAnchor(IPv4, anchor, rules, tableName);
        installAnchor(IPv6, anchor, rules, tableName);
        return;
    }

    const QString cmd = getCommand(ip);
    const QString anchorChain = QStringLiteral("%1.a.%2").arg(kAnchorName, anchor);
    const QString actualChain = QStringLiteral("%1.%2").arg(kAnchorName, anchor);

    // Start by defining a placeholder chain, which stays locked into place
    // in the root chain without being removed or recreated, ensuring the
    // intended precedence order.
    createChain(ip, anchorChain, tableName);
    linkChain(ip, anchorChain, kRootChain, false, tableName);

    // Create the actual rule chain, which we'll insert or remove from the
    // placeholder anchor when needed.
    createChain(ip, actualChain, tableName);
    for (const QString& rule : rules)
        execute(QStringLiteral("%1 -w -A %2 %3 -t %4").arg(cmd, actualChain, rule, tableName));
}

void IpTablesFirewall::uninstallAnchor(IpTablesFirewall::IPVersion ip, const QString& anchor, const QString& tableName)
{
    if (ip == Both)
    {
        uninstallAnchor(IPv4, anchor, tableName);
        uninstallAnchor(IPv6, anchor, tableName);
        return;
    }

    const QString cmd = getCommand(ip);
    const QString anchorChain = QStringLiteral("%1.a.%2").arg(kAnchorName, anchor);
    const QString actualChain = QStringLiteral("%1.%2").arg(kAnchorName, anchor);

    unlinkChain(ip, anchorChain, kRootChain, tableName);
    deleteChain(ip, anchorChain, tableName);
    deleteChain(ip, actualChain, tableName);
}

QStringList IpTablesFirewall::getDNSRules(const QString &adapterName, const QStringList& servers)
{
    if(adapterName.isEmpty())
    {
        qInfo() << "Adapter name not set, not applying DNS firewall rules";
        return {};
    }

    QStringList result;
    for (const QString& server : servers)
    {
        result << QStringLiteral("-o %1 -d %2 -p udp --dport 53 -j ACCEPT").arg(adapterName, server);
        result << QStringLiteral("-o %1 -d %2 -p tcp --dport 53 -j ACCEPT").arg(adapterName, server);
    }
    return result;
}

void IpTablesFirewall::install()
{
    // Clean up any existing rules if they exist.
    uninstall();

    // Create a root filter chain to hold all our other anchors in order.
    createChain(Both, kRootChain, kFilterTable);

    // Create a root raw chain
    createChain(Both, kRootChain, kRawTable);

    // Create a root NAT chain
    createChain(Both, kRootChain, kNatTable);

    // Create a root Mangle chain
    createChain(Both, kRootChain, kMangleTable);

    // Install our filter rulesets in each corresponding anchor chain.
    installAnchor(Both, QStringLiteral("000.allowLoopback"), {
        QStringLiteral("-o lo+ -j ACCEPT"),
    });
    installAnchor(Both, QStringLiteral("400.allowPIA"), {
        QStringLiteral("-m owner --gid-owner %1 -j ACCEPT").arg(kVpnGroupName),
    });

    // Allow all packets with the wireguard mark
    // Though another process could also mark packets with this fwmark to permit
    // them, it would have to have root privileges to do so, which means it
    // could install its own firewall rules anyway.
    installAnchor(Both, QStringLiteral("390.allowWg"), {
        QStringLiteral("-m mark --mark %1 -j ACCEPT").arg(Fwmark::wireguardFwmark),
    });
    installAnchor(Both, QStringLiteral("350.allowHnsd"), {
        // Updated at run-time in updateRules()
    });
    installAnchor(Both, QStringLiteral("350.cgAllowHnsd"), {
        // Port 13038 is the handshake control port
        QStringLiteral("-m owner --gid-owner %1 -m cgroup --cgroup %2 -p tcp --match multiport --dports 53,13038 -j ACCEPT").arg(kHnsdGroupName, CGroup::vpnOnlyId),
        QStringLiteral("-m owner --gid-owner %1 -m cgroup --cgroup %2 -p udp --match multiport --dports 53,13038 -j ACCEPT").arg(kHnsdGroupName, CGroup::vpnOnlyId),
        QStringLiteral("-m owner --gid-owner %1 -j REJECT").arg(kHnsdGroupName),
    });

    // block vpnOnly packets (these are only blocked when VPN is disconnected)
    installAnchor(Both, QStringLiteral("340.blockVpnOnly"), {
        QStringLiteral("-m cgroup --cgroup %1 -j REJECT").arg(CGroup::vpnOnlyId),
    });

    installAnchor(IPv4, QStringLiteral("320.allowDNS"), {});
    installAnchor(Both, QStringLiteral("310.blockDNS"), {
        QStringLiteral("-p udp --dport 53 -j REJECT"),
        QStringLiteral("-p tcp --dport 53 -j REJECT"),
    });

    installAnchor(Both, QStringLiteral("305.allowSubnets"), {
        // Updated at run-time
    });

    installAnchor(IPv4, QStringLiteral("300.allowLAN"), {
        QStringLiteral("-d 10.0.0.0/8 -j ACCEPT"),
        QStringLiteral("-d 169.254.0.0/16 -j ACCEPT"),
        QStringLiteral("-d 172.16.0.0/12 -j ACCEPT"),
        QStringLiteral("-d 192.168.0.0/16 -j ACCEPT"),
        QStringLiteral("-d 224.0.0.0/4 -j ACCEPT"),
        QStringLiteral("-d 255.255.255.255/32 -j ACCEPT"),
    });
    installAnchor(IPv6, QStringLiteral("300.allowLAN"), {
        QStringLiteral("-d fc00::/7 -j ACCEPT"),
        QStringLiteral("-d fe80::/10 -j ACCEPT"),
        QStringLiteral("-d ff00::/8 -j ACCEPT"),
    });
    installAnchor(IPv6, QStringLiteral("299.allowIPv6Prefix"), {
        // Updated at run-time
    });
    installAnchor(IPv4, QStringLiteral("290.allowDHCP"), {
        QStringLiteral("-p udp -d 255.255.255.255 --sport 68 --dport 67 -j ACCEPT"),
    });
    installAnchor(IPv6, QStringLiteral("290.allowDHCP"), {
        QStringLiteral("-p udp -d ff00::/8 --sport 546 --dport 547 -j ACCEPT"),
    });

    // This rule exists as the 100.blockAll rule can be toggled off if killswitch=off.
    // However we *always* want to block IPv6 traffic in any situation (until we properly support IPv6)
    installAnchor(IPv6, QStringLiteral("250.blockIPv6"), {
        QStringLiteral("! -o lo+ -j REJECT"),
    });

    installAnchor(Both, QStringLiteral("200.allowVPN"), {
        // To be added at runtime, dependent upon vpn method (i.e openvpn or wireguard)
    });

    installAnchor(Both, QStringLiteral("100.blockAll"), {
        QStringLiteral("-j REJECT"),
    });

    // NAT rules
    installAnchor(Both, QStringLiteral("100.transIp"), {
        // This anchor is set at run-time by split-tunnel ProcTracker class
    }, kNatTable);

    // Mangle rules
    // This rule is for "bypass subnets". The approach we use for
    // allowing subnets to bypass the VPN is to tag packets heading towards those subnets
    // with the "excludePacketTag" (same approach we use for bypass apps).
    // Interestingly, in order to allow correct interaction between bypass subnets and vpnOnly apps
    // ("vpnOnly apps always wins") we need to tag the subnets BEFORE we subsequently tag vpnOnly apps,
    // hence tagPkts appearing after tagSubnets. If we were to apply the tags the other way round, then the
    // "last tag wins", so the vpnOnly packet would have its tag replaced with excludePacketTag.
    // Tagging bypass subnets (with excludePacketTag) first means that vpnOnly tags get priority.
    installAnchor(Both, QStringLiteral("90.tagSubnets"), {
        // Updated at runtime
    }, kMangleTable);

    // Mangle rules
    installAnchor(Both, QStringLiteral("100.tagPkts"), {
        // Split tunnel
        QStringLiteral("-m cgroup --cgroup %1 -j MARK --set-mark %2").arg(CGroup::bypassId, Fwmark::excludePacketTag),

        // Inverse split tunnel
        QStringLiteral("-m cgroup --cgroup %1 -j MARK --set-mark %2").arg(CGroup::vpnOnlyId, Fwmark::vpnOnlyPacketTag)
    }, kMangleTable);


    // A rule to mitigate CVE-2019-14899 - drop packets addressed to the local
    // VPN IP but that are not actually received on the VPN interface.
    // See here: https://seclists.org/oss-sec/2019/q4/122
    installAnchor(Both, QStringLiteral("100.vpnTunOnly"), {
        // To be replaced at runtime
        QStringLiteral("-j ACCEPT")
    }, kRawTable);


    // Insert our filter root chain at the top of the OUTPUT chain.
    linkChain(Both, kRootChain, kOutputChain, true, kFilterTable);

    // Insert our NAT root chain at the top of the POSTROUTING chain.
    linkChain(Both, kRootChain, kPostRoutingChain, true, kNatTable);

    // Insert our Mangle root chain at the top of the OUTPUT chain.
    linkChain(Both, kRootChain, kOutputChain, true, kMangleTable);

    // Insert our Raw root chain at the top of the PREROUTING chain.
    linkChain(Both, kRootChain, kPreRoutingChain, true, kRawTable);
}

void IpTablesFirewall::uninstall()
{
    IpTablesFirewall::deactivate();

    // Filter chain
    unlinkChain(Both, kRootChain, kOutputChain, kFilterTable);
    deleteChain(Both, kRootChain, kFilterTable);

    // Raw chain
    unlinkChain(Both, kRootChain, kPreRoutingChain, kRawTable);
    deleteChain(Both, kRootChain, kRawTable);

    // NAT chain
    unlinkChain(Both, kRootChain, kPostRoutingChain, kNatTable);
    deleteChain(Both, kRootChain, kNatTable);

    // Mangle chain
    unlinkChain(Both, kRootChain, kOutputChain, kMangleTable);
    deleteChain(Both, kRootChain, kMangleTable);

    // Remove filter anchors
    uninstallAnchor(Both, QStringLiteral("000.allowLoopback"));
    uninstallAnchor(Both, QStringLiteral("400.allowPIA"));
    uninstallAnchor(Both, QStringLiteral("390.allowWg"));
    uninstallAnchor(Both, QStringLiteral("350.allowHnsd"));
    uninstallAnchor(Both, QStringLiteral("350.cgAllowHnsd"));
    uninstallAnchor(Both, QStringLiteral("340.blockVpnOnly"));
    uninstallAnchor(IPv4, QStringLiteral("320.allowDNS"));
    uninstallAnchor(Both, QStringLiteral("310.blockDNS"));
    uninstallAnchor(Both, QStringLiteral("305.allowSubnets"));
    uninstallAnchor(Both, QStringLiteral("300.allowLAN"));
    uninstallAnchor(IPv6, QStringLiteral("299.allowIPv6Prefix"));
    uninstallAnchor(Both, QStringLiteral("290.allowDHCP"));
    uninstallAnchor(IPv6, QStringLiteral("250.blockIPv6"));
    uninstallAnchor(Both, QStringLiteral("200.allowVPN"));
    uninstallAnchor(Both, QStringLiteral("100.blockAll"));

    // Remove Nat anchors
    uninstallAnchor(Both, QStringLiteral("100.transIp"), kNatTable);

    // Remove Mangle anchors
    uninstallAnchor(Both, QStringLiteral("90.tagSubnets"), kMangleTable);
    uninstallAnchor(Both, QStringLiteral("100.tagPkts"), kMangleTable);

    // Remove Raw anchors
    uninstallAnchor(Both, QStringLiteral("100.vpnTunOnly"), kRawTable);
}

void IpTablesFirewall::activate()
{
    // Ensure LAN traffic is always managed by the 'main' table.  This is needed
    // to ensure LAN routing for:
    // - Split tunnel.  Otherwise, split tunnel rules would send LAN traffic via
    //   the default gateway.
    // - Wireguard.  Otherwise, LAN traffic would be sent via the Wireguard
    //   interface.
    //
    // This has no effect for OpenVPN without split tunnel, or when disconnected
    // without split tunnel.
    //
    // Note that we use "suppress_prefixlength 1", not 0 as is typical, because
    // we also suppress the /1 gateway override routes applied by OpenVPN.
    execute(QStringLiteral("ip rule add lookup main suppress_prefixlength 1 prio %1").arg(Routing::Priorities::suppressedMain));
}

void IpTablesFirewall::deactivate()
{
    execute(QStringLiteral("ip rule del lookup main suppress_prefixlength 1 prio %1").arg(Routing::Priorities::suppressedMain));
}

bool IpTablesFirewall::isInstalled()
{
    return execute(QStringLiteral("iptables -w -C %1 -j %2 2> /dev/null").arg(kOutputChain, kRootChain)) == 0;
}

void IpTablesFirewall::enableAnchor(IpTablesFirewall::IPVersion ip, const QString &anchor, const QString& tableName)
{
    if (ip == Both)
    {
        enableAnchor(IPv4, anchor, tableName);
        enableAnchor(IPv6, anchor, tableName);
        return;
    }
    const QString cmd = getCommand(ip);
    const QString ipStr = ip == IPv6 ? QStringLiteral("(IPv6)") : QStringLiteral("(IPv4)");

    execute(QStringLiteral("if %1 -w -C %5.a.%2 -j %5.%2 -t %4 2> /dev/null ; then echo '%2%3: ON' ; else echo '%2%3: OFF -> ON' ; %1 -w -A %5.a.%2 -j %5.%2 -t %4; fi").arg(cmd, anchor, ipStr, tableName, kAnchorName));
}

void IpTablesFirewall::replaceAnchor(IpTablesFirewall::IPVersion ip, const QString &anchor, const QStringList &newRules, const QString& tableName)
{
    if (ip == Both)
    {
        replaceAnchor(IPv4, anchor, newRules, tableName);
        replaceAnchor(IPv6, anchor, newRules, tableName);
        return;
    }
    const QString cmd = getCommand(ip);
    const QString ipStr = ip == IPv6 ? QStringLiteral("(IPv6)") : QStringLiteral("(IPv4)");


    execute(QStringLiteral("%1 -w -F %2.%3 -t %4").arg(cmd, kAnchorName, anchor, tableName));
    for(const auto &rule : newRules)
    {
        execute(QStringLiteral("%1 -w -A %2.%3 %4 -t %5").arg(cmd, kAnchorName, anchor, rule, tableName));
    }
}

void IpTablesFirewall::disableAnchor(IpTablesFirewall::IPVersion ip, const QString &anchor, const QString& tableName)
{
    if (ip == Both)
    {
        disableAnchor(IPv4, anchor, tableName);
        disableAnchor(IPv6, anchor, tableName);
        return;
    }
    const QString cmd = getCommand(ip);
    const QString ipStr = ip == IPv6 ? QStringLiteral("(IPv6)") : QStringLiteral("(IPv4)");
    execute(QStringLiteral("if ! %1 -w -C %5.a.%2 -j %5.%2 -t %4 2> /dev/null ; then echo '%2%3: OFF' ; else echo '%2%3: ON -> OFF' ; %1 -w -F %5.a.%2 -t %4; fi").arg(cmd, anchor, ipStr, tableName, kAnchorName));
}

bool IpTablesFirewall::isAnchorEnabled(IpTablesFirewall::IPVersion ip, const QString &anchor, const QString& tableName)
{
    const QString cmd = getCommand(ip);
    return execute(QStringLiteral("%1 -w -C %4.a.%2 -j %4.%2 -t %3 2> /dev/null").arg(cmd, anchor, tableName, kAnchorName)) == 0;
}

void IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPVersion ip, const QString &anchor, bool enabled, const QString &tableName)
{
    if (enabled)
        enableAnchor(ip, anchor, tableName);
    else
        disableAnchor(ip, anchor, tableName);
}

void IpTablesFirewall::updateRules(const FirewallParams &params)
{
    const QString &adapterName = params.adapter ? params.adapter->devNode() : QString{};
    qInfo() << "VPN interface:" << adapterName;
    const QString &ipAddress6 = params.netScan.ipAddress6();

    // DNS rules depend on both adapters and DNS servers, update if either has
    // changed
    if(params.effectiveDnsServers != _dnsServers || adapterName != _adapterName)
    {
        execute(QStringLiteral("iptables -w -F %1.320.allowDNS").arg(kAnchorName));
        // If the adapter name isn't set, getDNSRules() returns an empty list
        for (const QString& rule : getDNSRules(adapterName, params.effectiveDnsServers))
        {
            execute(QStringLiteral("iptables -w -A %1.320.allowDNS %2").arg(kAnchorName, rule));
        }
    }

    // These rules only depend on the adapter name
    if(adapterName != _adapterName)
    {
        if(adapterName.isEmpty())
        {
            // Don't know the adapter name, wipe out the rules
            qInfo() << "Clearing allowVPN and allowHnsd rules, adapter name is not known";
            replaceAnchor(IpTablesFirewall::Both, QStringLiteral("200.allowVPN"), {});
            replaceAnchor(IpTablesFirewall::Both, QStringLiteral("350.allowHnsd"), {});
        }
        else
        {
            replaceAnchor(IpTablesFirewall::Both, QStringLiteral("200.allowVPN"), { QStringLiteral("-o %1 -j ACCEPT").arg(adapterName) });
            replaceAnchor(IpTablesFirewall::Both, QStringLiteral("350.allowHnsd"), {
                QStringLiteral("-m owner --gid-owner %1 -o %2 -p tcp --match multiport --dports 53,13038 -j ACCEPT").arg(IpTablesFirewall::kHnsdGroupName, adapterName),
                QStringLiteral("-m owner --gid-owner %1 -o %2 -p udp --match multiport --dports 53,13038 -j ACCEPT").arg(IpTablesFirewall::kHnsdGroupName, adapterName),
                QStringLiteral("-m owner --gid-owner %1 -j REJECT").arg(IpTablesFirewall::kHnsdGroupName),
            });
        }
    }

    if(ipAddress6 != _ipAddress6)
    {
        if(ipAddress6.isEmpty())
        {
            qInfo() << "Clearing out allowIPv6Prefix rule, no global IPv6 addresses found";
            replaceAnchor(IpTablesFirewall::IPv6, QStringLiteral("299.allowIPv6Prefix"), {});
        }
        else
        {
            IpTablesFirewall::replaceAnchor(
                IpTablesFirewall::IPv6,
                QStringLiteral("299.allowIPv6Prefix"),
                {
                    // First 64 bits is the IPv6 Network Prefix. This prefix is shared by all IPv6 hosts on the LAN,
                    // so whitelisting it allows those hosts to communicate
                    QStringLiteral("-d %2/64 -j ACCEPT").arg(ipAddress6)
                }
            );
        }
    }

    updateBypassSubnets(IpTablesFirewall::IPv4, params.bypassIpv4Subnets, _bypassIpv4Subnets);
    updateBypassSubnets(IpTablesFirewall::IPv6, params.bypassIpv6Subnets, _bypassIpv6Subnets);

    _ipAddress6 = ipAddress6;
    _dnsServers = params.effectiveDnsServers;
    _adapterName = adapterName;
}

void IpTablesFirewall::updateBypassSubnets(IpTablesFirewall::IPVersion ipVersion, const QSet<QString> &bypassSubnets, QSet<QString> &oldBypassSubnets)
{
    if(bypassSubnets != oldBypassSubnets)
    {
        if(bypassSubnets.isEmpty())
        {
            QString versionString = ipVersion == IpTablesFirewall::IPv6 ? "IPv6" : "IPv4";
            qInfo() << "Clearing out" << versionString << "allowSubnets rule, no subnets found";
            replaceAnchor(ipVersion, QStringLiteral("305.allowSubnets"), {});

            // Clear out the rules for tagging bypass subnet packets
            if(ipVersion == IPv4)
            {
                qInfo() << "Clearing out 90.tagSubnets";
                replaceAnchor(ipVersion, QStringLiteral("90.tagSubnets"), {}, kMangleTable);
            }
        }
        else
        {
            QStringList subnetRules;
            for(const auto &subnet : bypassSubnets)
                subnetRules << QStringLiteral("-d %1 -j ACCEPT").arg(subnet);


            // If there's any IPv6 addresses then we also need to whitelist link-local and broadcast
            // as these address ranges are needed for IPv6 Neighbor Discovery.
            if(ipVersion == IPv6)
            {
                subnetRules << QStringLiteral("-d fe80::/10 -j ACCEPT");
                subnetRules << QStringLiteral("-d ff00::/8 -j ACCEPT");
            }

            IpTablesFirewall::replaceAnchor(ipVersion,
                                            QStringLiteral("305.allowSubnets"), subnetRules);

            // We tag all packets heading towards a bypass subnet. This tag (excludePacketTag) is
            // used by our routing policies to route traffic outside the VPN.
            if(ipVersion == IPv4)
            {
                qInfo() << "Should be setting 90.tagSubnets";
                QStringList subnetRules;
                for(const auto &subnet : bypassSubnets)
                   subnetRules << QStringLiteral("-d %1 -j MARK --set-mark %2").arg(subnet).arg(Fwmark::excludePacketTag);

                replaceAnchor(ipVersion, QStringLiteral("90.tagSubnets"), subnetRules, kMangleTable);
            }
        }
    }
    oldBypassSubnets = bypassSubnets;
}

int IpTablesFirewall::execute(const QString &command, bool ignoreErrors)
{
    static Executor iptablesExecutor{CURRENT_CATEGORY};
    return iptablesExecutor.bash(command, ignoreErrors);
}
#endif
