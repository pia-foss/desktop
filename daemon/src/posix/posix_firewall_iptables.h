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
#line HEADER_FILE("posix/posix_firewall_iptables.h")

#ifndef POSIX_FIREWALL_IPTABLES_H
#define POSIX_FIREWALL_IPTABLES_H
#pragma once

#ifdef Q_OS_LINUX

#include <QString>
#include <QStringList>
#include <QHostAddress>
#include "daemon.h"

struct FirewallParams;

class SplitDNSInfo
{
public:
    enum class SplitDNSType
    {
        Bypass,
        VpnOnly
    };

    static QString existingDNS(const DaemonState &state);
    static SplitDNSInfo infoFor(const FirewallParams &params, const DaemonState &state, SplitDNSType dnsType);

public:
    SplitDNSInfo() = default;
    SplitDNSInfo(const QString &dnsServer, const QString &cGroupId, const QString &sourceIp)
        : _dnsServer{dnsServer}, _cGroupId{cGroupId}, _sourceIp{sourceIp}
    {
    }

    bool operator==(const SplitDNSInfo &other) const
    {
        return dnsServer() == other.dnsServer() &&
            cGroupId() == other.cGroupId() &&
            sourceIp() == other.sourceIp();
    }
    bool operator!=(const SplitDNSInfo &other) const {return !(*this == other);}

    const QString &dnsServer() const { return _dnsServer; }
    const QString &cGroupId() const { return _cGroupId; }
    const QString &sourceIp() const { return _sourceIp; }

    bool isValid() const;

private:
    QString _dnsServer;
    QString _cGroupId;
    QString _sourceIp;
};

// Firewall implementation on Linux using iptables.  Note that this also handles
// some aspects of routing that are closely related to firewall rules.
class IpTablesFirewall
{
    CLASS_LOGGING_CATEGORY("iptables")
public:
    enum IPVersion { IPv4, IPv6, Both };
    // Iptables Table names
    static QString kFilterTable, kNatTable, kMangleTable, kRawTable;

    // Handshake group name
    static QString kHnsdGroupName;

    // PIA process group names
    static QString kVpnGroupName;

private:
    static int createChain(IPVersion ip, const QString& chain, const QString& tableName = kFilterTable);
    static int deleteChain(IPVersion ip, const QString& chain, const QString& tableName = kFilterTable);
    static int unlinkAndDeleteChain(IPVersion ip, const QString& chain, const QString &parent, const QString& tableName = kFilterTable);
    static int linkChain(IPVersion ip, const QString& chain, const QString& parent, bool mustBeFirst = false, const QString& tableName = kFilterTable);
    static int unlinkChain(IPVersion ip, const QString& chain, const QString& parent, const QString& tableName = kFilterTable);
    static void installAnchor(IPVersion ip, const QString& anchor, const QStringList& rules, const QString& tableName = kFilterTable, const QString &rootChain = kRootChain);
    static void uninstallAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable, const QString &rootChain = kRootChain);
    static QString rootChainFor(const QString &chain);
    // Generate iptables rules to permit DNS to the specified servers.
    // vpnAdapterName is used for non-local DNS; traffic is only permitted
    // through the tunnel.  For local DNS, we permit it on any adapter.
    // No rules are created if the VPN adapter name is not known yet.
    static QStringList getDNSRules(const QString &vpnAdapterName, const QStringList& servers);
    static int execute(const QString& command, bool ignoreErrors = false);
    void enableRouteLocalNet();
    void disableRouteLocalNet();
private:
    // Chain names
    static QString kOutputChain, kInputChain, kForwardChain, kRootChain, kPostRoutingChain, kPreRoutingChain;

public:
    // Install/uninstall the firewall anchors
    static void install();
    static void uninstall();
    static bool isInstalled();
    static void ensureRootAnchorPriority(IPVersion ip = Both);
    static void enableAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static void disableAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static bool isAnchorEnabled(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static void setAnchorEnabled(IPVersion ip, const QString& anchor, bool enabled, const QString& tableName = kFilterTable);
    static void replaceAnchor(IpTablesFirewall::IPVersion ip, const QString &anchor, const QStringList &newRules, const QString& tableName = kFilterTable);
    void updateRules(const FirewallParams &params);
    void updateBypassSubnets(IpTablesFirewall::IPVersion ipVersion, const QSet<QString> &bypassSubnets, QSet<QString> &oldBypassSubnets);
    QString existingDNS();

private:
    // Last state used by updateRules(); allows us to detect when the rules must
    // be updated
    SplitDNSInfo _routedDnsInfo;    // Last behavior applied for routed packet DNS
    SplitDNSInfo _appDnsInfo;   // Last behavior applied for app DNS (either bypass or VPN only)
    QString _adapterName;
    QStringList _dnsServers;
    QString _ipAddress6;
    QSet<QString> _bypassIpv4Subnets;
    QSet<QString> _bypassIpv6Subnets;
    QString _previousRouteLocalNet;
};

#endif

#endif // POSIX_FIREWALL_PF_H
