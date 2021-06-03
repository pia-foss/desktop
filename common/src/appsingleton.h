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

#include "common.h"
#line HEADER_FILE("appsingleton.h")

#ifndef APPSINGLETON_H
#define APPSINGLETON_H
#pragma once

#include <QSharedMemory>
#include <QString>

// AppSingleton is used by applications that are limited to one instance per
// session.  It uses shared memory to:
// - detect if a prior instance of this application is already running
// - optionally pass a requested resource back to the first instance
//
// Applications limited to one instance this way should create an AppSingleton
// and then check if a prior instance already existed using
// isAnotherInstanceRunning().
//
// If there is, the app should exit.  A launch resource can be passed to the
// existing instance using setLaunchResource() (the first instance then must be
// signaled to check it, which is not handled by AppSingleton.)
//
// AppSingleton's shared resources are specific to the exact executable path
// passed to the constructor.  If two copies of the app are executed from
// different paths, they act independently (both can run, and they do not send
// launch resources to each other).  (This is intentional, primarily to permit
// running dev and release builds at the same time.)
class COMMON_EXPORT AppSingleton : public QObject, public Singleton<AppSingleton>
{
    Q_OBJECT

private:
    QString _executablePath;
    // Shared memory used to indicate the running instance of this application.
    // AppSingleton writes our PID here if no prior instance is found.
    // We don't clear this PID if our process exits, since we have to be able to
    // handle a crashed process anyway that failed to clear its PID.  (If a
    // prior PID is found, we check that it's still running and is actually this
    // application before accepting it as a prior instance.)
    QSharedMemory _pidShare;
    // Shared memory used to pass a launch resource to the existing instance of
    // this app.
    QSharedMemory _resourceShare;

    bool lockResourceShare();
    bool unlockResourceShare();

public:
    AppSingleton();
    ~AppSingleton();

    // returns the pid of the original instance, if available.
    // -1 otherwise
    qint64 isAnotherInstanceRunning ();

    void setLaunchResource (QString url);
    QString getLaunchResource();
};

extern template class COMMON_EXPORT_TMPL_SPEC_DECL Singleton<AppSingleton>;

#endif
