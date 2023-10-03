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

#pragma once
#include "linux_fwmark.h"
#include "linux_routing.h"
#include "../firewallconfig.h"
#include <kapps_net/net.h>
#include <kapps_core/src/util.h>
#include <string>
#include <kapps_core/src/fs.h>
#include <assert.h>

namespace kapps { namespace net {

class KAPPS_NET_EXPORT CGroupIds
{
public:
    CGroupIds(const FirewallConfig &config);

public:
    const std::string &bypassId() const { return _bypassId;}
    const std::string &vpnOnlyId() const { return _vpnOnlyId;}

    // Setup the bypass and vpnOnly cgroups + routing rules
    void setupNetCls();

    // Remove the cgroup routing rules - we do not need to remove the cgroups
    // as they have no impact without the routing rules
    void teardownNetCls();
    
    // Get the configured fwmark values; CGroupIds owns this because they also
    // determine the cgroup IDs
    const Fwmark &fwmark() const {return _fwmark;}
    // CGroupIds also owns the Routing configuration because it sets up the
    // cgroup routing tables
    const Routing &routing() const {return _routing;}

private:
    Fwmark _fwmark;
    Routing _routing;

    // CGroup identifiers (net_cls.classid)
    const std::string _bypassId;
    const std::string _vpnOnlyId;

    const std::string _bypassFile;
    const std::string _vpnOnlyFile;
    // This is the parent cgroup file.
    // Writing to the parent cgroup is the canonical
    // way to remove an element from a specific cgroup.
    // Putting a process into this cgroup gives it default routing behavior.
    const std::string _defaultFile;
};

namespace CGroup
{
    // Actually make the net_cls cgroup directory and mount the VFS.
    // This function is only called if the host system does not already have a net_cls VFS
    bool KAPPS_NET_EXPORT createNetCls(const std::string &netClsDir);
    void KAPPS_NET_EXPORT addPidToCgroup(pid_t pid, const std::string &cGroupPath);
    void KAPPS_NET_EXPORT removePidFromCgroup(pid_t pid, const std::string &cGroupPath);
};

}}
