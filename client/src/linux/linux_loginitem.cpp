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

#include "common.h"
#line SOURCE_FILE("linux/linux_loginitem.cpp")
#include "path.h"
#include "version.h"
#include "brand.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>

bool linuxLaunchAtLogin()
{
    return QFile::exists(Path::ClientAutoStartFile);
}

void linuxSetLaunchAtLogin(bool required)
{
    bool active = linuxLaunchAtLogin();

    if (required == active)
    {
        qDebug () << "Required state same as current state. Ignoring. Required: " << required;
        return;
    }

    if (required)
    {
        // Ensure the autostart folder exists
        Path::ClientAutoStartFile.mkparent();

        QString launchFileName = "/usr/share/applications/" BRAND_CODE "vpn.desktop";
        QString sourceData;

        QFile sourceFile(launchFileName);

        if (sourceFile.open(QFile::ReadOnly | QIODevice::Text))
        {
            sourceData = QString::fromUtf8(sourceFile.readAll());
            sourceFile.close();
        }
        else
        {
            qError() << "Can't open" << launchFileName << "for reading!";
            return;
        }

        QFile file(Path::ClientAutoStartFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream stream(&file);
            stream << sourceData.replace(QRegularExpression("Exec=(.*)"), "Exec=\\1 --quiet");
        }
        else
        {
            qError() << "Can't open" << Path::ClientAutoStartFile << "for writing!";
            return;
        }
    }
    else
    {
        QFile::remove(Path::ClientAutoStartFile);
    }
}
