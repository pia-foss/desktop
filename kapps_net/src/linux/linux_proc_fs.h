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
#include <kapps_core/core.h>
#include <kapps_net/net.h>
#include <kapps_core/src/util.h>
#include <string>
#include <assert.h>
#include <unordered_set>
#include <functional>
#include <regex>
#include <dirent.h>
#include <kapps_core/src/fs.h>

// Convenience functions for working with the Linux /proc VFS
namespace ProcFs
{
    const std::string kProcDirName{"/proc"};

    // Return all pids for the given executable path
    std::unordered_set<pid_t> pidsForPath(const std::string &path, bool silent=false);

    // Return all (immediate) children pids of parentPid
    std::unordered_set<pid_t> childPidsOf(pid_t parentPid, bool silent=false);

    // Given a pid, return the launch path for the process.
    //
    // By default, errors from readlink() are traced, but this can be suppressed
    // with silent=true (used by ProcTracker when it enumerates all existing
    // processes, as it's common for transient processes to fail).
    std::string pathForPid(pid_t pid, bool silent=false);

    // Given a pid, get the mount namespace id for that pid.
    // This enables us to verify that a given path (returned from pidsForPath)
    // belongs to a namespace we accept - otherwise users can bypass
    // the VPN by setting up their own mount namespace and creating a process
    // that maps to one set to bypass the VPN in settings.json
    // Currently, we only accept mount namespaces that match the mount namespace of pia-daemon
    std::string mountNamespaceId(pid_t pid, bool silent=false);

    // Is pid a child of parentPid ?
    bool isChildOf(pid_t parentPid, pid_t pid, bool silent=false);

    // Get all PIDs that satisfy a predicate.
    template <typename Func_T>
    std::unordered_set<pid_t> filterPids(Func_T filterFunc, bool silent=false)
    {
        static const std::regex pidRegex{"[1-9]+"};

        std::unordered_set<pid_t> filteredPids;
        // Filter the file list by directories
        for(const auto &entry : kapps::core::fs::listFiles(kProcDirName, DT_DIR))
        {
            // Filter by directories that represent a process (pid)
            if(std::regex_search(entry, pidRegex))
            {
              pid_t pid = std::stoul(entry);
              if(filterFunc(pid))
                  filteredPids.insert(pid);
            }
        }

        return filteredPids;
    }

} // namespace ProcFs
