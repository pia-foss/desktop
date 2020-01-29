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
#line SOURCE_FILE("nativetray.cpp")

#include "nativetray.h"

#if defined(Q_OS_MACOS)
#include "mac/mac_tray.h"
#elif defined(Q_OS_WIN)
#include "win/win_tray.h"
#elif defined(Q_OS_LINUX)
#include "linux/nativetrayqt.h"
#endif

std::unique_ptr<NativeTray> NativeTray::create(IconState initialIcon, const QString &initialIconSet)
{
#if defined(Q_OS_MACOS)
    return createNativeTrayMac(initialIcon, initialIconSet);
#elif defined(Q_OS_WIN)
    return createNativeTrayWin(initialIcon, initialIconSet);
#elif defined(Q_OS_LINUX)
    return std::unique_ptr<NativeTray>{new NativeTrayQt{initialIcon, initialIconSet}};
#else
    #error No tray icon implementation available for this platform
#endif
}
