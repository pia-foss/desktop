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
#line HEADER_FILE("recursivedirwatcher.h")

#ifndef RECURSIVEDIRWATCHER_H
#define RECURSIVEDIRWATCHER_H

#include <QFileSystemWatcher>
#include "path.h"

// RecursiveWatcher watches a path to a file or directory that may or may not
// exist yet.  It watches all parent directories to detect when the target path
// is created.
//
// When any change occurs - in the file/directory specified or any parent -
// changed() is emitted with the path that was changed.
//
// RecursiveWatcher is best used for short-lived watches, such as while waiting
// for a child process to start up and create a file.  Watches of all parents up
// to the root might be expensive for long-lived watches.
class RecursiveWatcher : public QObject
{
    Q_OBJECT

public:
    RecursiveWatcher(Path target);

private:
    // Add watches to _fsWatcher for _target and all of its ancestors.
    void addRecursiveWatches();
    void pathChanged(const QString &path);

public:
    const Path &target() const {return _target;}

signals:
    // The specified file/directory, or any parent, was changed.  The path given
    // has the same semantics as QFileSystemWatcher::directoryChanged() or
    // QFileSystemWatcher::fileChanged() - that is, for a directory, a file was
    // added/removed in the specified directory (directory modified), or the
    // directory itself was removed.  For a file, the file itself was modified
    // or removed.
    //
    // In general, slots should usually ignore 'path' and just re-check
    // existence of the file/directory of interest.  When an ancestor directory
    // is created, it could already have contents (changed() won't be emitted
    // for children that already exist).  For example, an existing directory
    // could have been linked at the path specified.
    void changed(const QString &path);

private:
    Path _target;
    QFileSystemWatcher _fsWatcher;
};

#endif
