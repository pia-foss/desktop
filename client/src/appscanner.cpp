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
#line SOURCE_FILE("appscanner.cpp")

#include "appscanner.h"
#include <QFileInfo>
#include <QDirIterator>

#if defined(Q_OS_MAC)
#include "mac/mac_appscanner.h"
#elif defined(Q_OS_WIN)
#include "win/win_appscanner.h"
#elif defined(Q_OS_LINUX)
#include "linux/linux_appscanner.h"
#endif

void DummyAppScanner::scanApplications()
{
    qDebug () << "Scanning applications";

    QJsonArray applications;

    applications.append(SystemApplication(QStringLiteral("firefox.exe"), QStringLiteral("Firefox"), {}).toJsonObject());
    applications.append(SystemApplication(QStringLiteral("inkscape.exe"), QStringLiteral("Inkscape"), {}).toJsonObject());
    applications.append(SystemApplication(QStringLiteral("audacity.exe"), QStringLiteral("Audacity2"), {}).toJsonObject());

    emit applicationScanComplete(applications);
}

std::unique_ptr<AppScanner> AppScanner::create()
{
#if defined(Q_OS_MAC)
    return std::unique_ptr<AppScanner>{new MacAppScanner()};
#elif defined(Q_OS_WIN)
    return std::unique_ptr<AppScanner>{new WinAppScanner()};
#elif defined(Q_OS_LINUX)
    return std::unique_ptr<AppScanner>{new LinuxAppScanner()};
#else
    return std::unique_ptr<AppScanner>{new DummyAppScanner{}};
#endif
}
