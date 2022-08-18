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

#pragma once
#include <libproc.h>  // for proc_pidpath()
#include <set>
#include <vector>
#include <kapps_net/net.h>
#include <kapps_core/core.h>
#include <kapps_core/src/logger.h>
#include "../originalnetworkscan.h"      // For OriginalNetworkScan
#include "mac_splittunnel_types.h"

namespace kapps { namespace net {
class KAPPS_NET_EXPORT AddressAndPort
{
public:
    AddressAndPort(std::uint32_t ip, std::uint16_t port)
    : _ip{ip}
    , _port{port}
    {}

    AddressAndPort(const AddressAndPort &other)
    : AddressAndPort(other._ip, other._port)
    {}

    bool operator==(const AddressAndPort &other) const { return _ip == other._ip && _port == other._port; }
    bool operator!=(const AddressAndPort &other) const { return !(*this == other); }
    bool operator<(const AddressAndPort &other) const {return _ip < other._ip;}

public:
    std::uint32_t ip() const {return _ip;}
    std::uint16_t port() const {return _port;}
private:
    std::uint32_t _ip;
    std::uint16_t _port;
};

namespace PortFinder
{
    // The maximum number of PIDs we support
enum { maxPids = 16384 };

std::set<pid_t> KAPPS_NET_EXPORT pids(const std::vector<std::string> &paths);
PortSet KAPPS_NET_EXPORT ports(const std::set<pid_t> &pids, IPVersion ipVersion, const OriginalNetworkScan &netScan);
PortSet KAPPS_NET_EXPORT ports(const std::vector<std::string> &paths, IPVersion ipVersion, const OriginalNetworkScan &netScan);
std::set<AddressAndPort> KAPPS_NET_EXPORT addresses4(const std::vector<std::string> &paths);
pid_t KAPPS_NET_EXPORT pidForPort(std::uint16_t port, IPVersion ipVersion=IPv4);

bool KAPPS_NET_EXPORT matchesPath(const std::vector<std::string> &paths, pid_t pid);
std::string KAPPS_NET_EXPORT pidToPath(pid_t);

template <typename Func_T>
pid_t pidFor(Func_T func)
{
    int totalPidCount = 0;
    std::vector<pid_t> allPidVector;
    allPidVector.resize(maxPids);

    // proc_listallpids() returns the total number of PIDs in the system
    // (assuming that maxPids is > than the total PIDs, otherwise it returns maxPids)
    totalPidCount = proc_listallpids(allPidVector.data(), maxPids * sizeof(pid_t));

    for (int i = 0; i != totalPidCount; ++i)
    {
        pid_t pid = allPidVector[i];

        // Add the PID to our set if matches one of the paths
        if(func(pid))
            return pid;
    }

    return 0;
}

template <typename Func_T>
std::set<pid_t> pidsFor(Func_T func)
{
    int totalPidCount = 0;
    std::set<pid_t> pidsForPaths;
    std::vector<pid_t> allPidVector;
    allPidVector.resize(maxPids);

    // proc_listallpids() returns the total number of PIDs in the system
    // (assuming that maxPids is > than the total PIDs, otherwise it returns maxPids)
    totalPidCount = proc_listallpids(allPidVector.data(), maxPids * sizeof(pid_t));

    if(totalPidCount == maxPids)
    {
        KAPPS_CORE_WARNING() << "Reached max PID count" << maxPids
            << "- some processes may not be identified";
    }

    for(int i = 0; i != totalPidCount; ++i)
    {
        pid_t pid = allPidVector[i];

        // Add the PID to our set if matches one of the paths
        if(func(pid))
            pidsForPaths.insert(pid);
    }

    return pidsForPaths;
}
}

}}
