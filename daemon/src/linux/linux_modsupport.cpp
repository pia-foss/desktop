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
#line SOURCE_FILE("linux_modsupport.cpp")

#include "linux_modsupport.h"
#include "exec.h"
#include <QFile>
#include <QTimer>
#include <sys/utsname.h>

LinuxModSupport::LinuxModSupport()
    : _signalQueued{false}
{
    utsname kernelName{};
    uname(&kernelName);

    QString kernelRelease{QString::fromUtf8(kernelName.release)};
    qInfo() << "Kernel release:" << kernelRelease;

    Path modulesDir{QStringLiteral("/lib/modules/") + kernelRelease};
    // Watch the containing directory
    if(!_fsWatcher.addPath(modulesDir))
    {
        qWarning() << "Unable to watch directory" << modulesDir;
    }
    // Watch the modules.dep file itself
    _modulesDepPath = modulesDir / QStringLiteral("modules.dep");
    if(!_fsWatcher.addPath(_modulesDepPath))
    {
        qWarning() << "Unable to watch file" << _modulesDepPath;
    }

    connect(&_fsWatcher, &QFileSystemWatcher::directoryChanged, this,
        [this]()
        {
            // It's unlikely that modules.dep is ever added or removed, but try
            // to add it again just in case
            _fsWatcher.addPath(_modulesDepPath);
            queueModulesUpdated();
        });
    connect(&_fsWatcher, &QFileSystemWatcher::fileChanged, this,
        [this](){queueModulesUpdated();});
}

void LinuxModSupport::queueModulesUpdated()
{
    // We usually get a whole bunch of file changes in very quick succession
    // when a change occurs.  Wait 100ms from the first change to skip most of
    // the duplicates.
    if(!_signalQueued)
    {
        qInfo() << "Module change occurred, wait before emitting update";
        _signalQueued = true;
        QTimer::singleShot(100, this,
            [this]()
            {
                qInfo() << "Emit module update now";
                _signalQueued = false;
                emit modulesUpdated();
            });
    }
}

bool LinuxModSupport::hasModule(const QString &module)
{
    // Check if the module is present as a loaded module.  This always detects
    // built-in modules (since they're always loaded), although it's probably
    // unlikely that any module of interest is built into the kernel.
    if(QFile::exists(QStringLiteral("/sys/module/") + module))
    {
        qInfo() << "Module" << module << "is loaded";
        return true;
    }
    // Check if the module is available to load using modprobe.  This detects
    // loadable modules even if they aren't loaded, but can't detect built-in
    // modules.
    if(Exec::cmd(QStringLiteral("modprobe"), {QStringLiteral("--show-depends"), module}) == 0)
        return true;

    // Not available
    return false;
}
