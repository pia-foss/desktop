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

#include <common/src/common.h>
#line SOURCE_FILE("linux_env.cpp")

#include "linux_env.h"
#include "linux_language.h"
#include "linux_scaler.h"

namespace LinuxEnv {

namespace
{
    Desktop _desktop = Desktop::Unknown;
    bool _desktopRtl = false;

    Desktop detectDesktop(const QProcessEnvironment &procEnv)
    {
        // Use the XDG_CURRENT_DESKTOP environment variable to figure out what
        // desktop environment is being used.
        const auto &desktopEnv = procEnv.value(QStringLiteral("XDG_CURRENT_DESKTOP"));

        qInfo() << "XDG_CURRENT_DESKTOP=" << desktopEnv;
        // GNOME variants can be reported as "GNOME" (Debian, Antergos/Arch),
        // "GNOME-Classic:GNOME" (Fedora 29), "ubuntu:GNOME" (Ubuntu 18.04), or
        // "GNOME-Flashback:GNOME" (Arch wiki, not verified).  Accept anything
        // ending with ":GNOME" or exactly "GNOME".
        if(desktopEnv == QStringLiteral("GNOME") || desktopEnv.endsWith(":GNOME"))
            return Desktop::GNOME;
        if(desktopEnv == QStringLiteral("Unity"))   // Verified Ubuntu 16.04
            return Desktop::Unity;
        if(desktopEnv == QStringLiteral("XFCE"))    // Verified Xubuntu 18.04
            return Desktop::XFCE;
        if(desktopEnv == QStringLiteral("KDE"))     // Verified Kubuntu 18.04, 16.04
            return Desktop::KDE;
        if(desktopEnv == QStringLiteral("LXDE"))    // Verified Lubuntu 18.04, 16.04
            return Desktop::LXDE;
        if(desktopEnv == QStringLiteral("LXQt"))    // Verified Debian 9.5
            return Desktop::LXQt;
        if(desktopEnv == QStringLiteral("Deepin"))  // Verified Antergos/Arch
            return Desktop::Deepin;
        if(desktopEnv == QStringLiteral("X-Cinnamon"))  // Verified Mint 19.1
            return Desktop::Cinnamon;
        if(desktopEnv == QStringLiteral("MATE"))    // Verified Mint 19.1
            return Desktop::MATE;
        if(desktopEnv == QStringLiteral("Pantheon"))    // Verified elementaryOS 5
            return Desktop::Pantheon;
        return Desktop::Unknown;
    }

    bool detectDesktopRtl()
    {
        // Figure out if the desktop is RTL by checking if the primary display
        // language is an RTL language.
        auto displayLanguages = linuxGetDisplayLanguages();
        if(displayLanguages.empty())
            return false;
        // QLocale doesn't necessarily preserve the country/script correctly for
        // an arbitrary language/country/script input, but this shouldn't affect
        // the RTL test.
        return QLocale{displayLanguages[0]}.textDirection() == Qt::LayoutDirection::RightToLeft;
    }
}

void preAppInit()
{
    LinuxWindowScaler::preAppInit();
    const auto &procEnv = QProcessEnvironment::systemEnvironment();
    _desktop = detectDesktop(procEnv);
    _desktopRtl = detectDesktopRtl();
    qInfo() << "Detected desktop" << qEnumToString(_desktop)
        << "and RTL:" << _desktopRtl;
}

Desktop getDesktop()
{
    return _desktop;
}

bool isDesktopRtl()
{
    return _desktopRtl;
}

}
