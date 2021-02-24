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
#line HEADER_FILE("linux_env.h")

#ifndef LINUX_ENV_H
#define LINUX_ENV_H

namespace LinuxEnv {

Q_NAMESPACE // For Q_ENUM_NS() below

// Do initialization that must occur before the QGuiAppliation is created.
void preAppInit();

// Possible desktop environments that we can detect
enum class Desktop
{
    Unknown,    // Some other environment
    GNOME,
    Unity,
    XFCE,
    KDE,
    LXDE,
    LXQt,
    Deepin,
    Cinnamon,
    MATE,
    Pantheon
};
Q_ENUM_NS(Desktop)

// Get the desktop environment we detected.  Use sparingly; needed for a few
// tray behaviors that don't have a sensible default across all environments.
// Valid after preAppInit().
Desktop getDesktop();

// Check whether the desktop language is RTL.  Use sparingly too; used by the
// tray icon to determine the likely position of the tray (if there is one).
bool isDesktopRtl();

}

#endif
