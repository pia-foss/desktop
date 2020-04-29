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
#line SOURCE_FILE("linux_cgroup.cpp")

#include "linux_cgroup.h"
#include "linux_fwmark.h"
#include "linux_routing.h"
#include "exec.h"
#include "path.h"
#include "brand.h"

#include <QDir>

const QString CGroup::bypassId{hexNumberStr(BRAND_LINUX_CGROUP_BASE)};
const QString CGroup::vpnOnlyId{hexNumberStr(BRAND_LINUX_CGROUP_BASE+1)};

bool CGroup::createNetCls()
{
    Path netClsDir{Path::ParentVpnExclusionsFile.parent()};
    Path netClsParentDir{netClsDir.parent()};

    qWarning() << "The directory" << netClsDir
               << "is not found, but is required by the split tunnel feature."
               << "Attempting to create.";

    // create net_cls directory and mount VFS
    if(mkdirNetCls(netClsParentDir) && mountNetCls(netClsDir))
    {
        qInfo() << "Successfully created" << netClsDir;
        return true;
    }
    else
    {
        qWarning() << "Failed to create" << netClsDir;
        return false;
    }
}

void CGroup::setupNetCls()
{
    const QString bypassDir{Path::VpnExclusionsFile.parent()};
    const QString vpnOnlyDir{Path::VpnOnlyFile.parent()};

    // Split tunnel (exclusions)
    setupCgroup(bypassDir, bypassId, Fwmark::excludePacketTag, Routing::bypassTable);
    // Inverse split tunnel (vpn only)
    setupCgroup(vpnOnlyDir, vpnOnlyId, Fwmark::vpnOnlyPacketTag, Routing::vpnOnlyTable);
}

void CGroup::teardownNetCls()
{
    teardownCgroup(Fwmark::excludePacketTag, Routing::bypassTable);
    teardownCgroup(Fwmark::vpnOnlyPacketTag, Routing::vpnOnlyTable);
}

void CGroup::setupCgroup(const Path &cGroupDir, const QString &cGroupId, const QString &packetTag, const QString &routingTableName)
{
    qInfo() << "Should be setting up cgroups in" << cGroupDir << "for traffic splitting";

    // Create the net_cls group
    execute(QStringLiteral("if [ ! -d %1 ] ; then mkdir %1 ; sleep 0.1 ; echo %2 > %1/net_cls.classid ; fi")
        .arg(cGroupDir).arg(cGroupId));
    // Set a rule with priority 100 (lower priority than local but higher than main/default, 0 is highest priority)
    execute(QStringLiteral("if ! ip rule list | grep -q %1 ; then ip rule add from all fwmark %1 lookup %2 pri 100 ; fi")
        .arg(packetTag, routingTableName));
}

void CGroup::teardownCgroup(const QString &packetTag, const QString &routingTableName)
{
    qInfo() << "Tearing down cgroup and routing rules";
    execute(QStringLiteral("if ip rule list | grep -q %1; then ip rule del from all fwmark %1 lookup %2 2> /dev/null ; fi")
        .arg(packetTag, routingTableName));
    execute(QStringLiteral("ip route flush table %1").arg(routingTableName));
    execute(QStringLiteral("ip route flush cache"));
}

bool CGroup::mountNetCls(const QString &netClsDir)
{
    return 0 == execute(QStringLiteral("mount -t cgroup -o net_cls none %1").arg(netClsDir), false);
}

bool CGroup::mkdirNetCls(const QString &parentDir)
{
    return QDir{parentDir}.mkdir(QStringLiteral("net_cls"));
}

int CGroup::execute(const QString &command, bool ignoreErrors)
{
    static Executor iptablesExecutor{CURRENT_CATEGORY};
    return iptablesExecutor.bash(command, ignoreErrors);
}
