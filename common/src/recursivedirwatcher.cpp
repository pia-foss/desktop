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
#line SOURCE_FILE("recursivedirwatcher.cpp")

#include "recursivedirwatcher.h"

RecursiveWatcher::RecursiveWatcher(Path target)
    : _target{std::move(target)}
{
    // RecursiveDirWatcher builds on Windows, but it can't be used right now due
    // to incorrect behavior in Path::parent().  (Path::parent() for "C:\aaa\"
    // returns "C:", not "C:\" - "C:" means "the current working directory for
    // drive C:".)
#ifdef Q_OS_WIN
    Q_ASSERT(false);
#endif

#ifdef Q_OS_MACOS
    // Use a short-polling QTimer instead to work around QFileSystemWatcher
    // issues on macOS 10.13.
    connect(&_timer, &QTimer::timeout, this, &RecursiveWatcher::check);
    _timer.setSingleShot(false);
    _timer.setInterval(100);
    _timer.start();
#else
    connect(&_fsWatcher, &QFileSystemWatcher::directoryChanged, this,
            &RecursiveWatcher::pathChanged);
    addRecursiveWatches();
#endif
}

#ifndef Q_OS_MACOS
void RecursiveWatcher::pathChanged(const QString &path)
{
    // Any time a change occurs, add watches for all parents again.  We
    // can't watch a directory that doesn't exist, so we need to try
    // again if a directory was added.
    //
    // This means that even if a parent directory is created, then
    // contents are added, we won't emit changed() for the contents if
    // they are added before we observe the parent change.  For slots,
    // this is equivalent to linking in a directory that has contents -
    // the contents exist when the parent change was emitted, so we
    // don't emit changes for the children.
    addRecursiveWatches();
    qInfo() << "Change in path" << path << "watching"
        << _target;
    emit check();
};

void RecursiveWatcher::addRecursiveWatches()
{
    Path watchDir = _target;
    while(true)
    {
        _fsWatcher.addPath(watchDir);
        Path parent = watchDir.parent();
        if(parent == watchDir)
            break;  // We reached the filesystem root
        watchDir = std::move(parent);
    }
}
#endif
