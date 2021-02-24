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

#include "linux_proc_fs.h"
QSet<pid_t> ProcFs::pidsForPath(const QString &path)
{
    return filterPids([&](pid_t pid) { return pathForPid(pid) == path; });
}

QSet<pid_t> ProcFs::childPidsOf(pid_t parentPid)
{
    return filterPids([&](pid_t pid) { return isChildOf(parentPid, pid); });
}

QString ProcFs::pathForPid(pid_t pid)
{
    QString link = QStringLiteral("%1/%2/exe").arg(kProcDirName).arg(pid);
    return QFile::symLinkTarget(link);
}

bool ProcFs::isChildOf(pid_t parentPid, pid_t pid)
{
    static const QRegularExpression parentPidRegex{QStringLiteral("PPid:\\s+([0-9]+)")};

    QFile statusFile{QStringLiteral("%1/%2/status").arg(kProcDirName).arg(pid)};
    if(!statusFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    auto match = parentPidRegex.match(statusFile.readAll());
    if(match.hasMatch())
    {
        auto foundParentPid = match.captured(1).toInt();
        return foundParentPid == parentPid;
    }

    return false;
}
