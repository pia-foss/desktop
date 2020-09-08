// Copyright (c) 2020 Private Internet Access, Inc.
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
#line HEADER_FILE("filewatcher.h")

#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <QFileSystemWatcher>
#include "path.h"

// FileWatcher watches for a file to be modified, created, or deleted.
// QFileSystemWatcher can't be used directly for this, because it can only
// watch a path that actually exists.  If the file specified doesn't exist,
// FileWatcher will walk up the directory heirarchy to the first directory that
// does exist, and watch that to detect when the file might be created.
class FileWatcher : public QObject
{
    Q_OBJECT

public:
    FileWatcher(Path target);

private:
    // Add a watch to _fsWatcher for _target if it exists, or the first ancestor
    // that exists otherwise.
    void addWatch();
    void pathChanged(const QString &);

signals:
    // The specified file/directory might have changed.  Slots should try to
    // open the file to see if it exists and check its contents.
    //
    // This does not necessarily indicate that the file has always changed; in
    // particular, when the file does not exist, it just indicates that some
    // change in a parent directory has occurred.  FileWatcher does guarantee
    // that a change will be emitted when the file has actually changed, though.
    void changed();

private:
    Path _target;
    Path _currentWatch;
    QFileSystemWatcher _fsWatcher;
};

#endif
