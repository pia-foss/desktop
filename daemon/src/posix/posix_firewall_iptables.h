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

class IpTablesFirewall
{
    CLASS_LOGGING_CATEGORY("iptables")
public:
    enum IPVersion { IPv4, IPv6, Both };
    // Table names
    static QString kFilterTable, kNatTable, kMangleTable, kRtableName, kVpnOnlyRtableName, kRawTable;
public:
    using FilterCallbackFunc = std::function<void()>;
private:
    static int createChain(IPVersion ip, const QString& chain, const QString& tableName = kFilterTable);
    static int deleteChain(IPVersion ip, const QString& chain, const QString& tableName = kFilterTable);
    static int linkChain(IPVersion ip, const QString& chain, const QString& parent, bool mustBeFirst = false, const QString& tableName = kFilterTable);
    static int unlinkChain(IPVersion ip, const QString& chain, const QString& parent, const QString& tableName = kFilterTable);
    static void installAnchor(IPVersion ip, const QString& anchor, const QStringList& rules, const QString& tableName = kFilterTable, const FilterCallbackFunc& enableFunc = {}, const FilterCallbackFunc& disableFunc = {});
    static void uninstallAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static QStringList getDNSRules(const QStringList& servers);
    static void setupTrafficSplitting();
    static void teardownTrafficSplitting();
    static void setupCgroup(const Path &cGroupDir, QString cGroupId, QString packetTag, QString routingTableName);
    static void teardownCgroup(QString packetTag, QString routingTableName);
    static int execute(const QString& command, bool ignoreErrors = false);
private:
    // Chain names
    static QString kOutputChain, kRootChain, kPostRoutingChain, kPreRoutingChain;

public:
    static void install();
    static void uninstall();
    static bool isInstalled();
    static void ensureRootAnchorPriority(IPVersion ip = Both);
    static void enableAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static void disableAnchor(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static bool isAnchorEnabled(IPVersion ip, const QString& anchor, const QString& tableName = kFilterTable);
    static void setAnchorEnabled(IPVersion ip, const QString& anchor, bool enabled, const QString& tableName = kFilterTable);
    static void replaceAnchor(IpTablesFirewall::IPVersion ip, const QString &anchor, const QStringList &newRules, const QString& tableName);
    static void updateDNSServers(const QStringList& servers);
};

#endif

#endif // POSIX_FIREWALL_PF_H
