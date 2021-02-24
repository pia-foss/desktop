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

#include "common.h"
#line HEADER_FILE("linux_cgroup.h")

#ifndef LINUX_CGROUP_H
#define LINUX_CGROUP_H

class CGroup
{
   CLASS_LOGGING_CATEGORY("linux_cgroup")
public:
    // CGroup identifiers (net_cls.classid)
    const static QString bypassId;
    const static QString vpnOnlyId;

    // Setup the bypass and vpnOnly cgroups + routing rules
    static void setupNetCls();

    // Remove the cgroup routing rules - we do not need to remove the cgroups
    // as they have no impact without the routing rules
    static void teardownNetCls();

    // Actually make the net_cls cgroup directory and mount the VFS.
    // This function is only called if the host system does not already have a net_cls VFS
    static bool createNetCls();

public:
    static void addPidToCgroup(pid_t pid, const Path &cGroupPath);
    static void removePidFromCgroup(pid_t pid, const Path &cGroupPath);

private:
    static void writePidToCGroup(pid_t pid, const QString &cGroupPath);
    static void removeChildPidsFromCgroup(pid_t parentPid, const Path &cGroupPath);
    static void addChildPidsToCgroup(pid_t parentPid, const Path &cGroupPath);
    static void setupCgroup(const Path &cGroupDir, const QString &cGroupId, const QString &packetTag,
                            const QString &routingTableName, int priority);
    static void teardownCgroup(const QString &packetTag, const QString &routingTableName);
    static bool mkdirNetCls(const QString &parentDir);
    static bool mountNetCls(const QString &netClsDir);
    static int execute(const QString &command, bool ignoreErrors = false);
};

#endif
