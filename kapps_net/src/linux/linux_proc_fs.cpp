// Copyright (c) 2024 Private Internet Access, Inc.
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

#include "linux_proc_fs.h"
#include <unistd.h>

namespace ProcFs
{

std::unordered_set<pid_t> pidsForPath(const std::string &path, bool silent)
{
    return filterPids([&](pid_t pid) { return pathForPid(pid, silent) == path; });
}

std::unordered_set<pid_t> childPidsOf(pid_t parentPid, bool silent)
{
    return filterPids([&](pid_t pid) { return isChildOf(parentPid, pid, silent); });
}

std::string pathForPid(pid_t pid, bool silent)
{
    return kapps::core::fs::readLink(qs::format("%/%/exe", kProcDirName, pid), silent);
}

std::string mountNamespaceId(pid_t pid, bool silent)
{
    return kapps::core::fs::readLink(qs::format("%/%/ns/mnt", kProcDirName, pid), silent);
}

bool isChildOf(pid_t parentPid, pid_t pid, bool silent)
{
    static const std::regex parentPidRegex{"PPid:\\s+([0-9]+)"};

    std::string statusFile{qs::format("%/%/status", kProcDirName, pid)};
    // 50 buffer size large enough to hold any pid
    std::string statusContent{kapps::core::fs::readString(statusFile, 50, silent)};
    std::smatch match;

    std::regex_search(statusContent, match, parentPidRegex);
    if(match.size() >= 2)
    {
        auto foundParentPid = std::stoi(match.str(1));
        return foundParentPid == parentPid;
    }

    return false;
}
}
