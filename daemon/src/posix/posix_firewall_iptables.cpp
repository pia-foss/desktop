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

QString SplitDNSInfo::existingDNS(const DaemonState &state)
{
    auto existingDNSServers = state.existingDNSServers();
    if (existingDNSServers.empty())
    {
        return QStringLiteral("");
    }
    else
    {
        return QHostAddress{existingDNSServers.front()}.toString();
    }
}

SplitDNSInfo SplitDNSInfo::infoFor(const FirewallParams &params, const DaemonState &state, SplitDNSType splitType)
{
    QString dnsServer, cGroupId, sourceIp;

    if(splitType == SplitDNSType::Bypass)
    {
        dnsServer = existingDNS(state);
        cGroupId = CGroup::bypassId;
        sourceIp = params.netScan.ipAddress();
    }
    else
    {
        QStringList servers;
        if(params._connectionSettings)
            servers = params._connectionSettings->getDnsServers();
        dnsServer = servers.isEmpty() ? QString{} : servers.front();
        cGroupId = CGroup::vpnOnlyId;
        sourceIp = state.tunnelDeviceLocalAddress();
    }

    // Deal with loopback dns server special case - if dnsServer is loopback then sourceIp must be too
    if(QHostAddress{dnsServer}.isLoopback())
        sourceIp = QStringLiteral("127.0.0.1");

    return {dnsServer, cGroupId, sourceIp};
}

bool SplitDNSInfo::isValid() const
{
    return !_dnsServer.isEmpty() && !_cGroupId.isEmpty() && !_sourceIp.isEmpty();
}

namespace
{
    const QString kAnchorName{BRAND_CODE "vpn"};
}

QString IpTablesFirewall::kOutputChain = QStringLiteral("OUTPUT");
QString IpTablesFirewall::kInputChain = QStringLiteral("INPUT");
QString IpTablesFirewall::kForwardChain = QStringLiteral("FORWARD");
QString IpTablesFirewall::kPostRoutingChain = QStringLiteral("POSTROUTING");
QString IpTablesFirewall::kPreRoutingChain = QStringLiteral("PREROUTING");
QString IpTablesFirewall::kRootChain = QStringLiteral("%1.anchors").arg(kAnchorName);
QString IpTablesFirewall::kFilterTable = QStringLiteral("filter");
QString IpTablesFirewall::kNatTable = QStringLiteral("nat");
QString IpTablesFirewall::kRawTable = QStringLiteral("raw");
QString IpTablesFirewall::kMangleTable = QStringLiteral("mangle");
QString IpTablesFirewall::kVpnGroupName = BRAND_CODE "vpn";
QString IpTablesFirewall::kHnsdGroupName = BRAND_CODE "hnsd";

QString IpTablesFirewall::rootChainFor(const QString &chain)
{
    if(chain == QStringLiteral("OUTPUT"))
        return kRootChain;
    else
        return QStringLiteral("%1.%2").arg(kAnchorName, chain);
}

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

int IpTablesFirewall::unlinkAndDeleteChain(IpTablesFirewall::IPVersion ip, const QString& chain, const QString& parent, const QString& tableName)
{
    unlinkChain(ip, chain, parent, tableName);
    return deleteChain(ip, chain, tableName);
}

void IpTablesFirewall::ensureRootAnchorPriority(IpTablesFirewall::IPVersion ip)
{
    // Filter table
    linkChain(ip, rootChainFor("OUTPUT"), kOutputChain, true, kFilterTable);
    linkChain(ip, rootChainFor("FORWARD"), kForwardChain, true, kFilterTable);
    linkChain(ip, rootChainFor("INPUT"), kInputChain, true, kFilterTable);

    // Nat table
    linkChain(ip, rootChainFor("OUTPUT"), kOutputChain, true, kNatTable);
    linkChain(ip, rootChainFor("PREROUTING"), kPreRoutingChain, true, kNatTable);
    linkChain(ip, rootChainFor("POSTROUTING"), kPostRoutingChain, true, kNatTable);

    // Mangle table
    linkChain(ip, rootChainFor("OUTPUT"), kOutputChain, true, kMangleTable);
    linkChain(ip, rootChainFor("PREROUTING"), kPreRoutingChain, true, kMangleTable);

    // Raw table
    linkChain(ip, rootChainFor("PREROUTING"), kPreRoutingChain, true, kRawTable);
}

void IpTablesFirewall::installAnchor(IpTablesFirewall::IPVersion ip, const QString& anchor, const QStringList& rules, const QString& tableName, const QString &rootChain)
{

    if (ip == Both)
    {
        installAnchor(IPv4, anchor, rules, tableName, rootChain);
        installAnchor(IPv6, anchor, rules, tableName, rootChain);
        return;
    }

    const QString cmd = getCommand(ip);

    // iptables anchors in PIA are constructed from three chains, because we
    // need two "links" that we can replace or delete:
    // - The "anchor"-"actual" link is created or deleted to enable or disable
    //   this anchor
    // - The "actual"-"rules" link is replaced in order to atomically pivot the
    //   contents of the anchor
    //
    // The three chains are:
    // - The "anchor" chain (.a.) - stays locked into place in the root chain.
    //   The anchor is enabled or disabled by clearing or adding an anchor in
    //   this chain to the "actual" chain.  Maintaining fixed anchors preserves
    //   our desired precedence among rules.
    // - The "actual" chain (no infix) - acts as the linkage between the anchor
    //   and rule chains
    // - The "rule" chain (.r.) - contains the actual rules active at this point
    //   in time, renamed and then deleted if we pivot to a new set of rules
    const QString anchorChain = QStringLiteral("%1.a.%2").arg(kAnchorName, anchor);
    const QString actualChain = QStringLiteral("%1.%2").arg(kAnchorName, anchor);
    const QString ruleChain = QStringLiteral("%1.r.%2").arg(kAnchorName, anchor);

    // Define the anchor chain and link it into the parent chain
    createChain(ip, anchorChain, tableName);
    linkChain(ip, anchorChain, rootChain, false, tableName);

    // Define the actual chain - don't link it to the anchor chain yet, that
    // happens if this anchor is enabled
    createChain(ip, actualChain, tableName);

    // Define the rule chain, link it into the actual chain, and populate the
    // initial rules
    createChain(ip, ruleChain, tableName);
    linkChain(ip, ruleChain, actualChain, false, tableName);
    for (const QString& rule : rules)
        execute(QStringLiteral("%1 -w -A %2 %3 -t %4").arg(cmd, ruleChain, rule, tableName));
}

void IpTablesFirewall::uninstallAnchor(IpTablesFirewall::IPVersion ip, const QString& anchor, const QString& tableName, const QString &rootChain)
{
    if (ip == Both)
    {
        uninstallAnchor(IPv4, anchor, tableName, rootChain);
        uninstallAnchor(IPv6, anchor, tableName, rootChain);
        return;
    }

    const QString cmd = getCommand(ip);
    const QString anchorChain = QStringLiteral("%1.a.%2").arg(kAnchorName, anchor);
    const QString actualChain = QStringLiteral("%1.%2").arg(kAnchorName, anchor);

    unlinkChain(ip, anchorChain, rootChain, tableName);
    deleteChain(ip, anchorChain, tableName);
    deleteChain(ip, actualChain, tableName);
}

QStringList IpTablesFirewall::getDNSRules(const QString &vpnAdapterName, const QStringList& servers)
{
    if(vpnAdapterName.isEmpty())
    {
        qInfo() << "Adapter name not set, not applying DNS firewall rules";
        return {};
    }

    QStringList result;
    for (const QString& server : servers)
    {
        // If this is a local DNS server, allow it on any adapter; we don't
        // know which adapter would be used.  If it's a non-local DNS server,
        // restrict it to the VPN interface.
        //
        // Invalid addresses (if they occur somehow) are treated as non-local by
        // default.  If the address is unparseable, Ipv4Address is set to
        // 0.0.0.0, which is non-local.
        QString restrictAdapter{};
        if(!Ipv4Address{server}.isLocalDNS())
            restrictAdapter = QStringLiteral("-o ") + vpnAdapterName;
        result << QStringLiteral("%1 -d %2 -p udp --dport 53 -j ACCEPT").arg(restrictAdapter, server);
        result << QStringLiteral("%1 -d %2 -p tcp --dport 53 -j ACCEPT").arg(restrictAdapter, server);
    }
    return result;
}

void IpTablesFirewall::install()
{
    // Clean up any existing rules if they exist.
    uninstall();

    // Create a root filter chain to hold all our other anchors in order.
    createChain(Both, kRootChain, kFilterTable);
    createChain(Both, rootChainFor("FORWARD"), kFilterTable);
    createChain(Both, rootChainFor("INPUT"), kFilterTable);

    // Create root raw chains
    createChain(Both, rootChainFor("PREROUTING"), kRawTable);

    // Create root NAT chains
    createChain(Both, kRootChain, kNatTable);
    createChain(Both, rootChainFor("PREROUTING"), kNatTable);
    createChain(Both, rootChainFor("POSTROUTING"), kNatTable);

    // Create root Mangle chains
    createChain(Both, kRootChain, kMangleTable);
    createChain(Both, rootChainFor("PREROUTING"), kMangleTable);

    // Install our filter rulesets in each corresponding anchor chain.

    // Don't allow unfettered loopback traffic - in particular do not just
    // permit loopback DNS traffic. This is due to an obscure iptables issue
    // where a vpnOnly app which re-uses a port previously used by a non vpnOnly
    // (within the conntrack timeout) has its traffic routed the same way
    // as that non vpnOnly app - outside the VPN.
    // This is an issue generally for UDP, but in paricular for DNS traffic.
    // If the non vpnOnly app was on loopback and DNS uses loopback (as in the case of systemd)
    // then just allowing all loopback traffic would also allow the incorrectly routed vpnOnly DNS packets.
    // To work around this, we only allow non DNS loopback traffic and have the DNS traffic fall back
    // to the DNS rules found in 320.allowDNS.
    installAnchor(Both, QStringLiteral("000.allowLoopback"), {
        // Use -j RETURN so that the port 53 packets are handled by
        // later chains. Allow everything else.
        QStringLiteral("-o lo+ -p udp -m udp --dport 53 -j RETURN"),
        QStringLiteral("-o lo+ -p tcp -m tcp --dport 53 -j RETURN"),
        QStringLiteral("-o lo+ -j ACCEPT")
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

    installAnchor(IPv4, QStringLiteral("230.allowBypassApps"), {
        QStringLiteral("-m cgroup --cgroup %1 -j ACCEPT").arg(CGroup::bypassId, Fwmark::excludePacketTag),
    });

    installAnchor(Both, QStringLiteral("200.allowVPN"), {
        // To be added at runtime, dependent upon vpn method (i.e openvpn or wireguard)
    });

    installAnchor(Both, QStringLiteral("100.blockAll"), {
        QStringLiteral("-j REJECT"),
    });

    // NAT rules
    installAnchor(Both, QStringLiteral("80.splitDNS"), {
        // Updated dynamically (see updateRules)
    }, kNatTable, rootChainFor("OUTPUT"));

    installAnchor(Both, QStringLiteral("80.fwdSplitDNS"), {
        // Updated dynamically (see updateRules)
    }, kNatTable, rootChainFor("PREROUTING"));

    installAnchor(Both, QStringLiteral("90.snatDNS"), {
        // Updated dynamically (see updateRules)
    }, kNatTable, rootChainFor("POSTROUTING"));

    installAnchor(Both, QStringLiteral("90.fwdSnatDNS"), {
        // Updated dynamically (see updateRules)
    }, kNatTable, rootChainFor("POSTROUTING"));

    installAnchor(Both, QStringLiteral("100.transIp"), {
        // This anchor is set at run-time by split-tunnel ProcTracker class
    }, kNatTable, rootChainFor("POSTROUTING"));

    // Protect our loopback ips from outside access (since we may have route_local activated)
    installAnchor(IPv4, QStringLiteral("100.protectLoopback"), {
        QStringLiteral("! -i lo -o lo -j REJECT")
    }, kFilterTable, rootChainFor("INPUT"));

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

    installAnchor(Both, QStringLiteral("100.tagBypass"), {
        // Split tunnel
        QStringLiteral("-m cgroup --cgroup %1 -j MARK --set-mark %2").arg(CGroup::bypassId, Fwmark::excludePacketTag),
    }, kMangleTable);

    // Mangle rules
    installAnchor(Both, QStringLiteral("100.tagVpnOnly"), {
        // Inverse split tunnel
        QStringLiteral("-m cgroup --cgroup %1 -j MARK --set-mark %2").arg(CGroup::vpnOnlyId, Fwmark::vpnOnlyPacketTag)
    }, kMangleTable);

    // Marks all forwarded packets
    installAnchor(Both, QStringLiteral("100.tagFwd"), {
        QStringLiteral("-j MARK --set-mark %1").arg(Fwmark::forwardedPacketTag)
    }, kMangleTable, rootChainFor("PREROUTING"));

    // Mark forwarded packets to bypass IPs as "bypass" packets instead
    installAnchor(Both, QStringLiteral("200.tagFwdSubnets"), {
        // Updated at runtime
    }, kMangleTable, rootChainFor("PREROUTING"));

    // A rule to mitigate CVE-2019-14899 - drop packets addressed to the local
    // VPN IP but that are not actually received on the VPN interface.
    // See here: https://seclists.org/oss-sec/2019/q4/122
    installAnchor(Both, QStringLiteral("100.vpnTunOnly"), {
        // To be replaced at runtime
        QStringLiteral("-j ACCEPT")
    }, kRawTable, rootChainFor("PREROUTING"));

    // Insert our output filter root chain at the top of the OUTPUT chain.
    linkChain(Both, rootChainFor("OUTPUT"), kOutputChain, true, kFilterTable);

    // Insert our forward filter root chain at the top of the FORWARD chain
    linkChain(Both, rootChainFor("FORWARD"), kForwardChain, true, kFilterTable);

    // Insert our NAT root chain at the top of the POSTROUTING chain.
    linkChain(Both, rootChainFor("OUTPUT"), kOutputChain, true, kNatTable);
    linkChain(Both, rootChainFor("POSTROUTING"), kPostRoutingChain, true, kNatTable);
    linkChain(Both, rootChainFor("PREROUTING"), kPreRoutingChain, true, kNatTable);

    // Insert our Mangle root chain at the top of the OUTPUT chain.
    linkChain(Both, rootChainFor("OUTPUT"), kOutputChain, true, kMangleTable);
    linkChain(Both, rootChainFor("PREROUTING"), kPreRoutingChain, true, kMangleTable);

    // Insert our Raw root chain at the top of the PREROUTING chain.
    linkChain(Both, rootChainFor("PREROUTING"), kPreRoutingChain, true, kRawTable);

    linkChain(Both, rootChainFor("INPUT"), kInputChain, true, kFilterTable);

    // Ensure LAN traffic is always managed by the 'main' table.  This is needed
    // to ensure LAN routing for:
    // - Split tunnel.  Otherwise, split tunnel rules would send LAN traffic via
    //   the default gateway.
    // - Wireguard.  Otherwise, LAN traffic would be sent via the Wireguard
    //   interface.
    //
    // This has no effect for OpenVPN without split tunnel, or when disconnected
    // without split tunnel.  We may need this even if the daemon is not active,
    // because some split tunnel rules are still applied even when inactive
    // ("only VPN" rules).
    //
    // Note that we use "suppress_prefixlength 1", not 0 as is typical, because
    // we also suppress the /1 gateway override routes applied by OpenVPN.
    execute(QStringLiteral("ip rule add lookup main suppress_prefixlength 1 prio %1").arg(Routing::Priorities::suppressedMain));
    execute(QStringLiteral("ip -6 rule add lookup main suppress_prefixlength 1 prio %1").arg(Routing::Priorities::suppressedMain));

    // Route forwarded packets
    execute(QStringLiteral("ip rule add from all fwmark %1 lookup %2 prio %3").arg(Fwmark::forwardedPacketTag).arg(Routing::forwardedTable).arg(Routing::Priorities::forwarded));
    execute(QStringLiteral("ip -6 rule add from all fwmark %1 lookup %2 prio %3").arg(Fwmark::forwardedPacketTag).arg(Routing::forwardedTable).arg(Routing::Priorities::forwarded));
}

void IpTablesFirewall::uninstall()
{

    execute(QStringLiteral("ip rule del lookup main suppress_prefixlength 1 prio %1").arg(Routing::Priorities::suppressedMain));
    execute(QStringLiteral("ip -6 rule del lookup main suppress_prefixlength 1 prio %1").arg(Routing::Priorities::suppressedMain));

    // Remove forwarded packets policy
    execute(QStringLiteral("ip rule del from all fwmark %1 lookup %2 prio %3").arg(Fwmark::forwardedPacketTag).arg(Routing::forwardedTable).arg(Routing::Priorities::forwarded));
    execute(QStringLiteral("ip -6 rule del from all fwmark %1 lookup %2 prio %3").arg(Fwmark::forwardedPacketTag).arg(Routing::forwardedTable).arg(Routing::Priorities::forwarded));

    // Filter table
    unlinkAndDeleteChain(Both, rootChainFor("OUTPUT"), kOutputChain, kFilterTable);
    unlinkAndDeleteChain(Both, rootChainFor("FORWARD"), kForwardChain, kFilterTable);
    unlinkAndDeleteChain(Both, rootChainFor("INPUT"), kInputChain, kFilterTable);

    // NAT table
    unlinkAndDeleteChain(Both, rootChainFor("OUTPUT"), kOutputChain, kNatTable);
    unlinkAndDeleteChain(Both, rootChainFor("PREROUTING"), kPreRoutingChain, kNatTable);
    unlinkAndDeleteChain(Both, rootChainFor("POSTROUTING"), kPostRoutingChain, kNatTable);

    // Mangle table
    unlinkAndDeleteChain(Both, rootChainFor("OUTPUT"), kOutputChain, kMangleTable);
    unlinkAndDeleteChain(Both, rootChainFor("PREROUTING"), kPreRoutingChain, kMangleTable);

    // Raw table
    unlinkAndDeleteChain(Both, rootChainFor("PREROUTING"), kPreRoutingChain, kRawTable);

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
    uninstallAnchor(IPv4, QStringLiteral("230.allowBypassApps"));
    uninstallAnchor(Both, QStringLiteral("200.allowVPN"));
    uninstallAnchor(Both, QStringLiteral("100.blockAll"));
    uninstallAnchor(Both, QStringLiteral("100.protectLoopback"));

    // Remove Nat anchors
    uninstallAnchor(Both, QStringLiteral("90.snatDNS"), kNatTable, rootChainFor("POSTROUTING"));
    uninstallAnchor(Both, QStringLiteral("100.transIp"), kNatTable, rootChainFor("POSTROUTING"));
    uninstallAnchor(Both, QStringLiteral("90.fwdSnatDNS"), kNatTable, rootChainFor("POSTROUTING"));
    uninstallAnchor(Both, QStringLiteral("80.fwdSplitDNS"), kNatTable, rootChainFor("PREROUTING"));
    uninstallAnchor(Both, QStringLiteral("80.splitDNS"), kNatTable, rootChainFor("OUTPUT"));

    // Remove Mangle anchors
    uninstallAnchor(Both, QStringLiteral("90.tagSubnets"), kMangleTable, rootChainFor("OUTPUT"));
    uninstallAnchor(Both, QStringLiteral("100.tagBypass"), kMangleTable, rootChainFor("OUTPUT"));
    uninstallAnchor(Both, QStringLiteral("100.tagVpnOnly"), kMangleTable, rootChainFor("OUTPUT"));
    uninstallAnchor(Both, QStringLiteral("200.tagFwdSubnets"), kMangleTable, rootChainFor("PREROUTING"));
    uninstallAnchor(Both, QStringLiteral("100.tagFwd"), kMangleTable, rootChainFor("PREROUTING"));

    // Remove Raw anchors
    uninstallAnchor(Both, QStringLiteral("100.vpnTunOnly"), kRawTable, rootChainFor("PREROUTING"));
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

    // To replace the anchor atomically:
    // 1. Rename the old "rule" chain (see model in installAnchor())
    // 2. Define a new "rule" chain and populate it
    // 3. Replace the anchor in the "actual" chain to point to the new rule chain
    //    ^ This is key, this atomically pivots from one rule set to the other.
    // 4. Flush and delete the old chain

    // Rename the old chain
    execute(QStringLiteral("%1 -w -E %2.r.%3 %2.o.%3 -t %4").arg(cmd, kAnchorName, anchor, tableName));
    // Create a new rule chain
    createChain(ip, QStringLiteral("%2.r.%3").arg(kAnchorName, anchor), tableName);
    // Populate the new chain
    for(const auto &rule : newRules)
    {
        execute(QStringLiteral("%1 -w -A %2.r.%3 %4 -t %5").arg(cmd, kAnchorName, anchor, rule, tableName));
    }
    // Pivot the actual chain to the new rule chain.  The actual chain should always have
    // exactly 1 rule (the anchor to the rule chain).
    execute(QStringLiteral("%1 -w -R %2.%3 1 -j %2.r.%3 -t %4").arg(cmd, kAnchorName, anchor, tableName));

    // Clean up - flush and delete the old chain
    deleteChain(ip, QStringLiteral("%2.o.%3").arg(kAnchorName, anchor), tableName);
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

// Ensure we can route 127.* so that we can rewrite source ips for DNS
void IpTablesFirewall::enableRouteLocalNet()
{
    if(!_previousRouteLocalNet.isEmpty())
        return; // Already enabled and stored the prior value

    _previousRouteLocalNet = Exec::bashWithOutput(QStringLiteral("sysctl -n 'net.ipv4.conf.all.route_localnet'"));
    if(!_previousRouteLocalNet.isEmpty())
    {
        if(_previousRouteLocalNet.toInt() != 1)
        {
            qInfo() << "Storing old net.ipv4.conf.all.route_localnet value:" << _previousRouteLocalNet;
            qInfo() << "Setting route_localnet to 1";
            execute(QStringLiteral("sysctl -w 'net.ipv4.conf.all.route_localnet=1'"));
        }
        else
        {
            qInfo() << "route_localnet already 1; nothing to do!";
        }
    }
    else
    {
        qWarning() << "Unable to store old net.ipv4.conf.all.route_localnet value";
    }
}

void IpTablesFirewall::disableRouteLocalNet()
{
    if(_previousRouteLocalNet.toInt() == 1)
    {
        qInfo() << "Previous route_localnet was" << _previousRouteLocalNet
            << "- nothing to restore";
    }
    else if(!_previousRouteLocalNet.isEmpty())
    {
        qInfo() << "Restoring route_localnet to: " << _previousRouteLocalNet;
        execute(QStringLiteral("sysctl -w 'net.ipv4.conf.all.route_localnet=%1'").arg(_previousRouteLocalNet));
    }
    _previousRouteLocalNet = "";
}

void IpTablesFirewall::updateRules(const FirewallParams &params)
{
    const QString &adapterName = params.adapter ? params.adapter->devNode() : QString{};
    qInfo() << "VPN interface:" << adapterName;
    const QString &ipAddress6 = params.netScan.ipAddress6();
    const auto &state = g_daemon->state();

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
            replaceAnchor(IPv6, QStringLiteral("299.allowIPv6Prefix"), {});
            replaceAnchor(IPv6, QStringLiteral("299.blockFwdIPv6Prefix"), {});
        }
        else
        {
            // First 64 bits is the IPv6 Network Prefix. This prefix is shared by all IPv6 hosts on the LAN,
            // so whitelisting it allows those hosts to communicate
            replaceAnchor(IPv6, QStringLiteral("299.allowIPv6Prefix"),
                          {QStringLiteral("-d %2/64 -j ACCEPT").arg(ipAddress6)});
            replaceAnchor(IPv6, QStringLiteral("299.blockFwdIPv6Prefix"),
                          {QStringLiteral("-d %2/64 -j REJECT").arg(ipAddress6)});
        }
    }

    updateBypassSubnets(IpTablesFirewall::IPv4, params.bypassIpv4Subnets, _bypassIpv4Subnets);
    updateBypassSubnets(IpTablesFirewall::IPv6, params.bypassIpv6Subnets, _bypassIpv6Subnets);

    // Manage DNS for forwarded packets
    SplitDNSInfo::SplitDNSType routedDns = SplitDNSInfo::SplitDNSType::VpnOnly;

    // We only want to use bypass DNS for routed packets if:
    // - Routed packets are set to bypass
    // - Split tunnel Name Servers are set to "Follow App Rules"
    if(params.enableSplitTunnel && !g_daemon->settings().routedPacketsOnVPN() && g_daemon->settings().splitTunnelDNS())
        routedDns = SplitDNSInfo::SplitDNSType::Bypass;
    SplitDNSInfo routedDnsInfo = SplitDNSInfo::infoFor(params, state, routedDns);

    // Since we can't control where routed DNS is addressed, always create rules
    // to force it to the DNS server specified.
    if(routedDnsInfo != _routedDnsInfo)
    {
        if(routedDnsInfo.isValid())
        {
            qInfo() << "Sending routed DNS to DNS server"
                << routedDnsInfo.dnsServer() << "via source IP" << routedDnsInfo.sourceIp();
            replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("90.fwdSnatDNS"), {
                QStringLiteral("-p udp --match mark --mark %1 -m udp --dport 53 -j SNAT --to-source %2").arg(Fwmark::forwardedPacketTag).arg(routedDnsInfo.sourceIp()),
                QStringLiteral("-p tcp --match mark --mark %1 -m tcp --dport 53 -j SNAT --to-source %2").arg(Fwmark::forwardedPacketTag).arg(routedDnsInfo.sourceIp())
                },
                kNatTable);

            replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("80.fwdSplitDNS"), {
                QStringLiteral("-p udp --match mark --mark %1 -m udp --dport 53 -j DNAT --to-destination %2:53").arg(Fwmark::forwardedPacketTag).arg(routedDnsInfo.dnsServer()),
                QStringLiteral("-p tcp --match mark --mark %1 -m tcp --dport 53 -j DNAT --to-destination %2:53").arg(Fwmark::forwardedPacketTag).arg(routedDnsInfo.dnsServer()),
                },
                kNatTable);
        }
        else
        {
            qInfo() << QStringLiteral("Not creating routed packet DNS rules, received empty value dnsServer: %1, sourceIp: %2").arg(routedDnsInfo.dnsServer(), routedDnsInfo.sourceIp());
            replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("90.fwdSnatDNS"),
                          {}, kNatTable);

            replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("80.fwdSplitDNS"),
                          {}, kNatTable);
        }

        _routedDnsInfo = routedDnsInfo;
    }

    // Manage split tunnel DNS.
    SplitDNSInfo appDnsInfo;
    // If we have to force either bypass or VPN-only apps to the correct DNS,
    // set up appDnsInfo appropriately so we can update the rules.
    if(params._connectionSettings)
    {
        // We never have to force _both_ kinds of apps' DNS
        Q_ASSERT(!(params._connectionSettings->forceVpnOnlyDns() && params._connectionSettings->forceBypassDns()));

        if(params._connectionSettings->forceVpnOnlyDns())
        {
            qInfo() << "Forcing VPN-only apps to our DNS";
            appDnsInfo = SplitDNSInfo::infoFor(params, state, SplitDNSInfo::SplitDNSType::VpnOnly);
        }
        else if(params._connectionSettings->forceBypassDns())
        {
            qInfo() << "Forcing bypass apps to existing DNS";
            appDnsInfo = SplitDNSInfo::infoFor(params, state, SplitDNSInfo::SplitDNSType::Bypass);
        }
    }

    if(appDnsInfo != _appDnsInfo)
    {
        if(appDnsInfo.isValid())
        {
            qInfo() << QStringLiteral("Updating split tunnel DNS due to network change: dnsServer: %1, cgroupId %2, sourceIp %3")
                .arg(appDnsInfo.dnsServer(), appDnsInfo.cGroupId(),
                     appDnsInfo.sourceIp());
            replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("90.snatDNS"), {
                QStringLiteral("-p udp -m cgroup --cgroup %1 -m udp --dport 53 -j SNAT --to-source %2").arg(appDnsInfo.cGroupId()).arg(appDnsInfo.sourceIp()),
                QStringLiteral("-p tcp -m cgroup --cgroup %1 -m tcp --dport 53 -j SNAT --to-source %2").arg(appDnsInfo.cGroupId()).arg(appDnsInfo.sourceIp()),
            },
            kNatTable);

            replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("80.splitDNS"), {
                QStringLiteral("-p udp -m cgroup --cgroup %1 -m udp --dport 53 -j DNAT --to-destination %2:53").arg(appDnsInfo.cGroupId()).arg(appDnsInfo.dnsServer()),
                QStringLiteral("-p tcp -m cgroup --cgroup %1 -m tcp --dport 53 -j DNAT --to-destination %2:53").arg(appDnsInfo.cGroupId()).arg(appDnsInfo.dnsServer()),
            },
            kNatTable);
        }
        else
        {
            qInfo() << QStringLiteral("Clear split tunnel DNS rules, don't have all information: dnsServer: %1, cgroupId %2, sourceIp %3")
                .arg(appDnsInfo.dnsServer(), appDnsInfo.cGroupId(),
                     appDnsInfo.sourceIp());
            replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("90.snatDNS"),
                          {}, kNatTable);

            replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("80.splitDNS"),
                          {}, kNatTable);
        }

        _appDnsInfo = appDnsInfo;
    }

    // DNS rules depend on both adapters and DNS servers, update if either has
    // changed
    QStringList effectiveDnsServers;
    if(params._connectionSettings)
        effectiveDnsServers = params._connectionSettings->getDnsServers();
    if(effectiveDnsServers != _dnsServers || adapterName != _adapterName)
    {
        // If the adapter name isn't set, getDNSRules() returns an empty list
        QStringList ruleList = getDNSRules(adapterName, effectiveDnsServers);

        if(!ruleList.isEmpty() && params.enableSplitTunnel)
        {
            // DNS leak protection for vpnOnly apps.
            // In rare situations, when making a DNS request, a vpnOnly app could re-use the port
            // previously used by a bypass app. When this happens, iptables causes the vpnOnly DNS request
            // to get routed the same way as the bypass request - causing a DNS leak.
            // We guard against this below.
            if(params._connectionSettings && params._connectionSettings->forceVpnOnlyDns())
            {
                const auto vpnOnlyServersStr = QStringList{appDnsInfo.dnsServer()}.join(',');
                // When the VPN does not have the default route, allow
                // the vpnOnly DNS servers
                ruleList << QStringLiteral("-p udp -m cgroup --cgroup %1 -m udp --dport 53 -d %2 -j ACCEPT").arg(CGroup::vpnOnlyId, vpnOnlyServersStr);
                ruleList << QStringLiteral("-p tcp -m cgroup --cgroup %1 -m tcp --dport 53 -d %2 -j ACCEPT").arg(CGroup::vpnOnlyId, vpnOnlyServersStr);
                // And block everything else
                // Doing this prevents a vpnOnly app re-using a port/route used by a bypass app
                ruleList << QStringLiteral("-p udp -m cgroup --cgroup %1 -m udp --dport 53 -j REJECT").arg(CGroup::vpnOnlyId);
                ruleList << QStringLiteral("-p tcp -m cgroup --cgroup %1 -m tcp --dport 53 -j REJECT").arg(CGroup::vpnOnlyId);

                // Reject bypass apps from using vpnOnly DNS (prevents a bypass app re-using a vpnOnly port/route)
                // If we didn't block this, it may allow bypass apps to make DNS requests over the VPN - this isn't technically a 'leak'
                // but is still weird/unexpected behaviour, so we prevent it.
                ruleList << QStringLiteral("-p udp -m cgroup --cgroup %1 -m udp --dport 53 -d %2 -j REJECT").arg(CGroup::bypassId, vpnOnlyServersStr);
                ruleList << QStringLiteral("-p tcp -m cgroup --cgroup %1 -m tcp --dport 53 -d %2 -j REJECT").arg(CGroup::bypassId, vpnOnlyServersStr);
            }
            else // VPN has default route
            {
                const auto bypassServersStr = QStringList{appDnsInfo.dnsServer()}.join(",");
                // Allow configured DNS servers for bypass apps (VPN has the default route)

                // Only apply our bypass leak protection if we have bypass DNS servers
                // (we will not have bypass DNS servers if ST "Name Servers" is set to "Use VPN DNS Only" rather than "Follow App Rules")
                if(!bypassServersStr.isEmpty())
                {
                    ruleList << QStringLiteral("-p udp -m cgroup --cgroup %1 -m udp --dport 53 -d %2 -j ACCEPT").arg(CGroup::bypassId, bypassServersStr);
                    ruleList << QStringLiteral("-p tcp -m cgroup --cgroup %1 -m tcp --dport 53 -d %2 -j ACCEPT").arg(CGroup::bypassId, bypassServersStr);
                    // And block everything else
                    ruleList << QStringLiteral("-p udp -m cgroup --cgroup %1 -m udp --dport 53 -j REJECT").arg(CGroup::bypassId);
                    ruleList << QStringLiteral("-p tcp -m cgroup --cgroup %1 -m tcp --dport 53 -j REJECT").arg(CGroup::bypassId);

                    // When the VPN does have the default route, vpnOnly apps use the configured VPN DNS.
                    // However, vpnOnly apps could still leak if a DNS request re-uses the route used by
                    // by a prior bypass app. This happens when a vpnOnly app re-uses the source port of a bypass app within the UDP conntrack timeout.
                    // To guard against this we block bypass DNS servers for any packet that is not part of the bypass cgroup.
                    // NOTE: we cannot use the vpnOnly cgroup here as no apps are added to the vpnOnly cgroup when the VPN has
                    // the default route.
                    ruleList << QStringLiteral("-p udp -m cgroup ! --cgroup %1 -m udp --dport 53 -d %2 -j REJECT").arg(CGroup::bypassId, bypassServersStr);
                    ruleList << QStringLiteral("-p tcp -m cgroup ! --cgroup %1 -m tcp --dport 53 -d %2 -j REJECT").arg(CGroup::bypassId, bypassServersStr);
                }
            }
        }

        // Re-allow localhost DNS now we've plugged the leaks.
        // localhost DNS is important for systemd which uses a 127.0.0.53 DNS proxy
        // for all DNS traffic.
        ruleList << QStringLiteral("-o lo+ -p udp -m udp --dport 53 -j ACCEPT");
        ruleList << QStringLiteral("-o lo+ -p tcp -m tcp --dport 53 -j ACCEPT");

        replaceAnchor(IpTablesFirewall::IPv4, QStringLiteral("320.allowDNS"), ruleList);
    }

    // Enable localhost routing
    // Without this option we cannot route our DNS packet if the source IP was
    // originally localhost, this is because the routing decision
    // i.e localhost vs routable ip is made BEFORE we rewrite the source IP in POSTROUTING
    if(params.enableSplitTunnel)
        enableRouteLocalNet();
    else
        disableRouteLocalNet();

    _adapterName = adapterName;
    _ipAddress6 = ipAddress6;
    _dnsServers = effectiveDnsServers;
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
            qInfo() << "Clearing out 200.tagFwdSubnets";
            replaceAnchor(ipVersion, QStringLiteral("200.tagFwdSubnets"), {}, kMangleTable);
        }
        else
        {
            QStringList subnetAcceptRules;
            for(const auto &subnet : bypassSubnets)
                subnetAcceptRules << QStringLiteral("-d %1 -j ACCEPT").arg(subnet);


            // If there's any IPv6 addresses then we also need to whitelist link-local and broadcast
            // as these address ranges are needed for IPv6 Neighbor Discovery.
            if(ipVersion == IPv6)
            {
                subnetAcceptRules << QStringLiteral("-d fe80::/10 -j ACCEPT");
                subnetAcceptRules << QStringLiteral("-d ff00::/8 -j ACCEPT");
            }

            IpTablesFirewall::replaceAnchor(ipVersion,
                                            QStringLiteral("305.allowSubnets"), subnetAcceptRules);

            QStringList subnetMarkRules;
            for(const auto &subnet : bypassSubnets)
               subnetMarkRules << QStringLiteral("-d %1 -j MARK --set-mark %2").arg(subnet).arg(Fwmark::excludePacketTag);
            // We tag all packets heading towards a bypass subnet. This tag (excludePacketTag) is
            // used by our routing policies to route traffic outside the VPN.
            if(ipVersion == IPv4)
            {
                qInfo() << "Setting 90.tagSubnets";

                replaceAnchor(ipVersion, QStringLiteral("90.tagSubnets"), subnetMarkRules, kMangleTable);
            }

            // For routed connections to bypassed subnets, apply the
            // bypass mark so they will always be routed to the original
            // gateway, regardless of the routed connection setting.
            replaceAnchor(ipVersion, QStringLiteral("200.tagFwdSubnets"), subnetMarkRules, kMangleTable);
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
