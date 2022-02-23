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


#include "common.h"
#include <QSet>
#include <QDir>

#ifndef PROC_FS_H
#define PROC_FS_H
// Convenience functions for working with the Linux /proc VFS
namespace ProcFs
{
    const QString kProcDirName{"/proc"};

    // Return all pids for the given executable path
    QSet<pid_t> pidsForPath(const QString &path);

    // Return all (immediate) children pids of parentPid
    QSet<pid_t> childPidsOf(pid_t parentPid);

    // Iterate and filter over all process PIDs in /proc
    QSet<pid_t> filterPids(const std::function<bool(pid_t)> &filterFunc);

    // Given a pid, return the launch path for the process
    QString pathForPid(pid_t pid);

    // Is pid a child of parentPid ?
    bool isChildOf(pid_t parentPid, pid_t pid);

    template <typename Func_T>
    QSet<pid_t> filterPids(Func_T filterFunc)
    {
        QDir procDir{kProcDirName};
        procDir.setFilter(QDir::Dirs);
        procDir.setNameFilters({"[1-9]*"});

        QSet<pid_t> filteredPids;
        for (const auto &entry : procDir.entryList())
        {
            pid_t pid = entry.toInt();
            if (filterFunc(pid))
                filteredPids.insert(pid);
        }

        return filteredPids;
    }

} // namespace ProcFs
#endif
