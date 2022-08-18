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
#line HEADER_FILE("recursivedirwatcher.h")

#ifndef RECURSIVEDIRWATCHER_H
#define RECURSIVEDIRWATCHER_H

#include <QFileSystemWatcher>
#include <QTimer>
#include "builtin/path.h"

// RecursiveWatcher watches a path to a file or directory that may or may not
// exist yet.  It watches all parent directories to detect when the target path
// is created.
//
// This is usually used for short-lived watches when the caller expects to see
// a file created (say, by a child process).  The caller should re-check the
// target file whenever check() is emitted - this is a 'hint' that a change may
// have occurred.
//
// On Linux, this is properly implemented using a QFileSystemWatcher, which is
// the ideal behavior - we don't do any extra work polling the file, and we
// don't have to wait for a poll interval to elapse before checking again.
//
// On macOS, crashes were observed on macOS 10.13 using Qt 5.15.2, it appears
// that a change in Qt may have caused this to start crashing on 10.13:
//   https://code.qt.io/cgit/qt/qtbase.git/commit/src/corelib/io/qfilesystemwatcher_fsevents.mm?h=5.15.2&id=c6f0236892c0002b11512683754f2b22ae979eec
//
// To avoid this issue, this falls back to short-polling every 100ms on macOS.
// This isn't ideal, but the current uses of RecursiveWatcher are very short-
// lived anyway, so it is reasonable.
class COMMON_EXPORT RecursiveWatcher : public QObject
{
    Q_OBJECT

public:
    RecursiveWatcher(Path target);

private:
#ifndef Q_OS_MACOS
    // Add watches to _fsWatcher for _target and all of its ancestors.
    void addRecursiveWatches();
    void pathChanged(const QString &path);
#endif

public:
    const Path &target() const {return _target;}

signals:
    // The caller should re-check the file/directory of interest.
    //
    // On Linux, this is emitted when the path specified or any ancestor has
    // changed.  (It's still possible that the file doesn't exist, etc., since
    // more than one change could have occurred.)
    //
    // On macOS, this is emitted periodically with a timer to fall back to
    // short-polling due to the QFileSystemWatcher issues mentioned above.
    void check();

private:
    Path _target;
#ifdef Q_OS_MACOS
    QTimer _timer;
#else
    QFileSystemWatcher _fsWatcher;
#endif
};

#endif
