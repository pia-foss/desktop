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
#line SOURCE_FILE("mac/mac_loginitem.cpp")

#include "mac_loginitem.h"
#include "brand.h"
#include "path.h"
#include "version.h"
#include <QFile>
#include <QTextStream>

namespace
{

// Contents of the launch agent property list used to launch the client at
// login.  This is split in the middle so the path to the application can be
// inserted at runtime.
//
// clang apparently does not support multiline raw string literals (it spews
// "unterminated C++ string" errors), so as an ugly workaround these are
// written as a bunch of concatenated string literals with explicit line breaks.
const QString launchAgentPlistPrefix = QStringLiteral(
R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
R"(<!DOCTYPE plist PUBLIC "-//Apple/DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">)" "\n"
R"(<plist version="1.0">)" "\n"
R"(<dict>)" "\n"
R"(    <key>Label</key>)" "\n"
R"(    <string>)" BRAND_IDENTIFIER R"(.client</string>)" "\n"
R"(    <key>ProgramArguments</key>)" "\n"
R"(    <array>)" "\n"
R"(        <string>/usr/bin/open</string>)" "\n"
R"(        <string>-W</string>)" "\n"
R"(        <string>)");
const QString launchAgentPlistSuffix = QStringLiteral(
R"(</string>)" "\n"
R"(        <string>--args</string>)" "\n"
R"(        <string>--quiet</string>)" "\n"
R"(    </array>)" "\n"
R"(    <key>RunAtLoad</key>)" "\n"
R"(    <true/>)" "\n"
R"(</dict>)" "\n"
R"(</plist>)");

}

bool macLaunchAtLogin()
{
    return QFile::exists(Path::ClientLaunchAgentPlist);
}

void macSetLaunchAtLogin(bool enabled)
{
    if(enabled)
    {
        Path::ClientLaunchAgentPlist.mkparent();
        QFile plistFile{Path::ClientLaunchAgentPlist};
        if(plistFile.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
        {
            QTextStream{&plistFile} << launchAgentPlistPrefix << Path::BaseDir
                << launchAgentPlistSuffix;
        }
        else
        {
            qCritical() << "Can't open launch agent property list"
                << Path::ClientLaunchAgentPlist;
        }
    }
    else
    {
        if(!QFile::remove(Path::ClientLaunchAgentPlist))
        {
            qWarning() << "Can't remove launch agent property list"
                << Path::ClientLaunchAgentPlist;
        }
    }
}
