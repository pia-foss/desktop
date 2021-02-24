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


class COMMON_EXPORT AppSingleton : public QObject, public Singleton<AppSingleton>
{
    Q_OBJECT

private:
    QSharedMemory _pidShare;
    QSharedMemory _resourceShare;
    QString _executableName;

    bool lockResourceShare ();
    bool unlockResourceShare();

public:
    AppSingleton(const QString &executableName, QObject *parent = nullptr);
    ~AppSingleton();


    // returns the pid of the original instance, if available.
    // -1 otherwise
    qint64 isAnotherInstanceRunning ();

    void setLaunchResource (QString url);
    QString getLaunchResource();
};

extern template class COMMON_EXPORT_TMPL_SPEC_DECL Singleton<AppSingleton>;

#endif
