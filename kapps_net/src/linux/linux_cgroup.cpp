// Copyright (c) 2023 Private Internet Access, Inc.
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

#include "linux_cgroup.h"
#include <kapps_core/src/logger.h>
#include "linux_fwmark.h"
#include "linux_routing.h"
#include "linux_proc_fs.h"
#include <kapps_core/src/posix/posix_objects.h>
#include <kapps_core/src/newexec.h>
#include <kapps_core/src/fs.h>

namespace fs = kapps::core::fs;

namespace kapps { namespace net {

namespace
{
    void setupCgroup(const std::string &cGroupDir, const std::string &cGroupId, const std::string &packetTag, const std::string &routingTableName, int priority)
    {
        KAPPS_CORE_INFO() << "Attempting to set up cgroups in" << cGroupDir << "for traffic splitting";

        // Create the net_cls group
        core::Exec::bash(qs::format("if [ ! -d % ] ; then mkdir % ; sleep 0.1 ; echo % > %/net_cls.classid ; fi", cGroupDir, cGroupDir, cGroupId, cGroupDir));
        core::Exec::bash(qs::format("if ! ip rule list | grep -q % ; then ip rule add from all fwmark % lookup % pri % ; fi", packetTag, packetTag, routingTableName, priority));
    }

    void teardownCgroup(const std::string &packetTag, const std::string &routingTableName)
    {
        KAPPS_CORE_INFO() << "Tearing down cgroup and routing rules";
        core::Exec::bash(qs::format("if ip rule list | grep -q %; then ip rule del from all fwmark % lookup % 2> /dev/null ; fi", packetTag, packetTag, routingTableName));
        core::Exec::bash(qs::format("ip route flush table %", routingTableName));
        core::Exec::bash("ip route flush cache");
    }

    bool mountNetCls(const std::string &netClsDir)
    {
        return 0 == core::Exec::bash(qs::format("mount -t cgroup -o net_cls none %", netClsDir), false);
    }

    void writePidToCGroup(pid_t pid, const std::string &cGroupPath)
    {
        fs::writeString(cGroupPath, std::to_string(pid));
    }

    void addChildPidsToCgroup(pid_t parentPid, const std::string &cGroupPath)
    {
        for(pid_t pid : ProcFs::childPidsOf(parentPid))
        {
            KAPPS_CORE_INFO() << "Adding child pid" << pid;
            CGroup::addPidToCgroup(pid, cGroupPath);
        }
    }

    void removeChildPidsFromCgroup(pid_t parentPid, const std::string &cGroupPath)
    {
        for(pid_t pid : ProcFs::childPidsOf(parentPid))
        {
            KAPPS_CORE_INFO() << "Removing child pid" << pid << cGroupPath;
            CGroup::removePidFromCgroup(pid, cGroupPath);
        }
    }
}

CGroupIds::CGroupIds(const FirewallConfig &config)
    : _fwmark{config.brandInfo.fwmarkBase}, _routing{config.brandInfo.code},
      _bypassId{hexNumberStr(config.brandInfo.cgroupBase)},
      _vpnOnlyId{hexNumberStr(config.brandInfo.cgroupBase+1)}, _bypassFile{config.bypassFile},
      _vpnOnlyFile{config.vpnOnlyFile}, _defaultFile{config.defaultFile}
{
    assert(config.brandInfo.cgroupBase);
    assert(!_bypassFile.empty());
    assert(!_vpnOnlyFile.empty());
    assert(!_defaultFile.empty());
}

void CGroupIds::setupNetCls()
{
    const std::string bypassDir{fs::dirName(_bypassFile)};
    const std::string vpnOnlyDir{fs::dirName(_vpnOnlyFile)};

    // Split tunnel (exclusions) - we want the bypass rule to have lower priority than the vpnOnly rule (see Routing::Priorities)
    // so that an app set to vpnOnly has all its packets sent over the VPN even if a bypass rule (such as a subnet bypass) would otherwise
    // allow those packets to escape the VPN. "vpnOnly" should always win.
    setupCgroup(bypassDir, _bypassId, _fwmark.excludePacketTag(), _routing.bypassTable(), 
        Routing::Priorities::bypass);
    // Inverse split tunnel (vpn only)
    setupCgroup(vpnOnlyDir, _vpnOnlyId, _fwmark.vpnOnlyPacketTag(), _routing.vpnOnlyTable(),
        Routing::Priorities::vpnOnly);
}

void CGroupIds::teardownNetCls()
{
    teardownCgroup(_fwmark.excludePacketTag(), _routing.bypassTable());
    teardownCgroup(_fwmark.vpnOnlyPacketTag(), _routing.vpnOnlyTable());
}

namespace CGroup
{
    bool createNetCls(const std::string &netClsDir)
    {
        KAPPS_CORE_WARNING() << "The directory" << netClsDir
                << "is not found, but is required by the split tunnel feature."
                << "Attempting to create.";

        // create net_cls directory and mount VFS
        if(core::fs::mkDir(netClsDir) && mountNetCls(netClsDir))
        {
            KAPPS_CORE_INFO() << "Successfully created" << netClsDir;
            return true;
        }
        else
        {
            KAPPS_CORE_WARNING() << "Failed to create" << netClsDir;
            return false;
        }
    }

    void addPidToCgroup(pid_t pid, const std::string &cGroupPath)
    {
        writePidToCGroup(pid, cGroupPath);
        // Add child processes (NOTE: we also recurse through child processes of child processes)
        addChildPidsToCgroup(pid, cGroupPath);
    }

    void removePidFromCgroup(pid_t pid, const std::string &cGroupPath)
    {
        // We remove a PID from a cgroup by adding it to its parent cgroup
        writePidToCGroup(pid, cGroupPath);
        // Remove child processes (NOTE: we also recurse through child processes of child processes)
        removeChildPidsFromCgroup(pid, cGroupPath);
    }
}

}}
