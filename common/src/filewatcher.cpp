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
#line SOURCE_FILE("filewatcher.cpp")

#include "filewatcher.h"

FileWatcher::FileWatcher(Path target)
    : _target{std::move(target)}
{
    // Like RecursiveDirWatcher, this builds on Windows but does not currently
    // work due to a bug in Path::parent().  It isn't currently needed on
    // Windows.
#ifdef Q_OS_WIN
    Q_ASSERT(false);
#endif

    connect(&_fsWatcher, &QFileSystemWatcher::directoryChanged, this,
            &FileWatcher::pathChanged);
    connect(&_fsWatcher, &QFileSystemWatcher::fileChanged, this,
            &FileWatcher::pathChanged);
    addWatch();
}

void FileWatcher::pathChanged(const QString &)
{
    addWatch();
    emit changed();
}

void FileWatcher::addWatch()
{
    if(!_currentWatch.str().isEmpty())
        _fsWatcher.removePath(_currentWatch);

    _currentWatch = _target;
    // Keep trying to watch the next parent until we find something that exists
    while(!_fsWatcher.addPath(_currentWatch))
    {
        // Didn't exist, go to the next parent
        Path parent = _currentWatch.parent();
        // Check if we reached the filesystem root - this generally shouldn't
        // happen, because the root should exist at the very least
        if(parent == _currentWatch)
        {
            _currentWatch = Path{};
            break;
        }
        _currentWatch = std::move(parent);
    }

    if(_currentWatch.str().isEmpty())
    {
        qWarning() << "Could not find any parent of" << _target
            << "that exists to watch for changes";
    }
    else if(_currentWatch == _target)
    {
        qInfo() << "Watching" << _target << "for changes";
    }
    else
    {
        qInfo() << "File" << _target << "does not exist, watch" << _currentWatch
            << "for the file to be created";
    }
}
