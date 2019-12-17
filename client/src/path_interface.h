// Copyright (c) 2019 London Trust Media Incorporated
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

#ifndef PATHINTERFACE_H
#define PATHINTERFACE_H

#include <QObject>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include "path.h"

class PathInterface : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QString daemonLogFile() { return Path::DaemonLogFile; }
    Q_INVOKABLE QString clientLogFile() { return Path::ClientLogFile; }
    Q_INVOKABLE QString regionOverrideFile() { return Path::DaemonSettingsDir / "region_override.json"; }
    Q_INVOKABLE QString bundledRegionFile() { return Path::ResourceDir / "servers.json"; }
    Q_INVOKABLE QString daemonSettingsDir() { return Path::DaemonSettingsDir; }
    Q_INVOKABLE QString clientSettingsDir() { return Path::ClientSettingsDir; }
    Q_INVOKABLE QString resourceDir() { return Path::ResourceDir; }
    Q_INVOKABLE QString daemonDataDir() { return Path::DaemonDataDir; }
    Q_INVOKABLE QString linuxAutoStartFile()
    {
#ifdef Q_OS_LINUX
        return Path::ClientAutoStartFile;
#else
        return QStringLiteral("N/A");
#endif
    }

    // duplicated from reporthelper.cpp ReportHelper::showFileInSystemViewer
    Q_INVOKABLE void showFileInSystemViewer(const QString &filePath)
    {
    #ifdef Q_OS_MAC
        QProcess::execute(QStringLiteral("/usr/bin/open"), { filePath });
    #endif

    #ifdef Q_OS_WIN
        QFileInfo fileInfo(filePath);
        QStringList args { QDir::toNativeSeparators(fileInfo.canonicalFilePath()) };
        QProcess::startDetached(QStringLiteral("explorer.exe"), args);
    #endif

    #ifdef Q_OS_LINUX
        if (QFile::exists(QStringLiteral("/usr/bin/xdg-open")))
            QProcess::startDetached(QStringLiteral("/usr/bin/xdg-open"), { filePath });
    #endif
    }
};
#endif
