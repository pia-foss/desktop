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
    // Check if there is already a net_cls mount - if so, attempting to mount another net_cls will fail.
    // We return the mount point if it exists, otherwise we return an empty string.
    std::string findPreExistingNetClsMount(const std::string &mountsFile)
    {
        // Use awk to find the mount point of an existing net_cls file system.
        // Example line from the mountsFile (/proc/mounts):
        // none /sys/fs/cgroup/net_cls cgroup rw,relatime,net_cls 0 0
        // We only care about field 2 (the mount point) and field 4 (options - we care about the existence of net_cls)
        const std::string preExistingNetClsDir = core::Exec::bashWithOutput(qs::format("awk '$4 ~ /net_cls/ { print $2 }' %", mountsFile));
        return preExistingNetClsDir;
    }

    // Create a symlink from our "pia" net_cls folder to the pre-existing net_cls mount point. We only do this if there is a pre-existing net_cls
    // mount, otherwise we create our own net_cls mount
    bool createNetClsSymlink(const std::string &preExistingNetClsDir, const std::string& netClsSymlink)
    {
        // Ensure we remove any existing pia net_cls directory (if necessary) to allow us to create the symlink
        if(core::fs::exists(netClsSymlink) && !(core::fs::readLink(netClsSymlink, true) == preExistingNetClsDir))
        {
            KAPPS_CORE_INFO() << "Removing PIA net_cls directory" << netClsSymlink << "to create new symlink" ;
            core::Exec::cmd("rm", {"-rf", netClsSymlink});
        }

        // Now create the symlink
        if(!core::fs::createSymlink(preExistingNetClsDir, netClsSymlink))
        {
            KAPPS_CORE_WARNING() << "Could not create symlink to net_cls location. Split tunnel will not work.";
            return false;
        }

        KAPPS_CORE_INFO() << "Created symlink to net_cls location!" << netClsSymlink << "->" << preExistingNetClsDir;
        return true;
    }

    // mountsFile defaults to /proc/mounts - see the header file
    bool createNetCls(const std::string &netClsDir, const std::string &mountsFile)
    {
        // First, We need to ensure the 'cgroup' parent folder exists
        const std::string parentFolder{core::fs::dirName(netClsDir)};
        if(!core::fs::dirExists(parentFolder))
        {
            KAPPS_CORE_INFO() << "First creating parent folder for net_cls" << parentFolder;
            core::fs::mkDir(parentFolder);
        }

        // First check for a pre-existing net_cls folder - if we find one then create
        // a symlink to it from our "pia" net_cls folder - if we don't find one then we
        // mount it ourselves.
        const std::string preExistingNetClsDir = findPreExistingNetClsMount(mountsFile);
        if(!preExistingNetClsDir.empty())
        {
            KAPPS_CORE_INFO() << "Found pre-existing net_cls mount-point:" << preExistingNetClsDir;
            return createNetClsSymlink(preExistingNetClsDir, netClsDir);
        }

        KAPPS_CORE_WARNING() << "The directory" << netClsDir
                << "is not found, but is required by the split tunnel feature."
                << "Attempting to create.";

        // create net_cls directory and mount VFS
        if(core::fs::mkDir_p(netClsDir) && mountNetCls(netClsDir))
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
