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
#line HEADER_FILE("posix/posix_firewall_iptables.h")

#ifndef POSIX_FIREWALL_IPTABLES_H
#define POSIX_FIREWALL_IPTABLES_H
#pragma once

#ifdef Q_OS_LINUX

#include <QString>
#include <QStringList>
#include <QHostAddress>

struct FirewallParams;

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

public:
    using FilterCallbackFunc = std::function<void()>;
private:
    static int createChain(IPVersion ip, const QString& chain, const QString& tableName = kFilterTable);
    static int deleteChain(IPVersion ip, const QString& chain, const QString& tableName = kFilterTable);
    static int linkChain(IPVersion ip, const QString& chain, const QString& parent, bool mustBeFirst = false, const QString& tableName = kFilterTable);
    static int unlinkChain(IPVersion ip, const QString& chain, const QString& parent, const QString& tableName = kFilterTable);
    static void installAnchor(IPVersion ip, const QString& anchor, const QStringList& rules, const QString& tableName = kFilterTable, const FilterCallbackFunc& enableFunc = {}, const FilterCallbackFunc& disableFunc = {});
    static void uninstallAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static QStringList getDNSRules(const QString &adapterName, const QStringList& servers);
    static void setupTrafficSplitting();
    static void teardownTrafficSplitting();
    static void setupCgroup(const Path &cGroupDir, QString cGroupId, QString packetTag, QString routingTableName);
    static void teardownCgroup(QString packetTag, QString routingTableName);
    static int execute(const QString& command, bool ignoreErrors = false);
private:
    // Chain names
    static QString kOutputChain, kRootChain, kPostRoutingChain, kPreRoutingChain;

public:
    // Install/uninstall the firewall anchors
    static void install();
    static void uninstall();
    // Activate/deactivate components that only become active when the daemon is
    // active.  Currently, this is just a routing rule (the firewall proper is
    // active even when the daemon is not active).
    static void activate();
    static void deactivate();
    static bool isInstalled();
    static void ensureRootAnchorPriority(IPVersion ip = Both);
    static void enableAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static void disableAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static bool isAnchorEnabled(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static void setAnchorEnabled(IPVersion ip, const QString& anchor, bool enabled, const QString& tableName = kFilterTable);
    static void replaceAnchor(IpTablesFirewall::IPVersion ip, const QString &anchor, const QStringList &newRules, const QString& tableName = kFilterTable);
    void updateRules(const FirewallParams &params);

private:
    // Last state used by updateRules(); allows us to detect when the rules must
    // be updated
    QString _adapterName;
    QStringList _dnsServers;
};

#endif

#endif // POSIX_FIREWALL_PF_H
