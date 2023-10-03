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

#include <common/src/common.h>
#line HEADER_FILE("linux_modsupport.h")

#ifndef LINUX_MODSUPPORT_H
#define LINUX_MODSUPPORT_H

#include <QFileSystemWatcher>
#include <common/src/builtin/path.h>

// LinuxModSupport detects whether the kernel has support for modules that can
// be used by PIA (currently just 'wireguard').  It watches modules.dep to
// detect when the user has installed or removed modules.
class LinuxModSupport : public QObject
{
    Q_OBJECT

public:
    LinuxModSupport();

private:
    void queueModulesUpdated();

public:
    // Test if a module is available (either built-in or loadable).
    bool hasModule(const QString &module);

signals:
    // The installed modules have been updated, re-check relevant modules.
    void modulesUpdated();

private:
    Path _modulesDepPath;
    QFileSystemWatcher _fsWatcher;
    bool _signalQueued;
};

#endif
