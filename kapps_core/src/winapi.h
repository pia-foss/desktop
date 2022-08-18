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

#include <kapps_core/core.h>
// Windows API headers are sensitive both to a inclusion order and to a
// number of macros used to exclude parts of the SDK.
// - NOMINMAX is critical to avoid min() and max() macros
// - Winsock 2 headers must be included before Windows.h to avoid conflicts with
//   Winsock 1.
//
// For internal usage that's not too bad, we would just include <win/winapi.h>
// everywhere we use the Windows SDK instead of the SDK headers.  Unfortunately,
// a handful of Qt headers also include Windows.h (QWindow, QQuickWindow), and
// as a result a number of our own headers have to include win/winapi.h on
// Windows before those headers.
//
// Rather than duplicate this logic over and over in all those headers, this
// winapi.h file is available on all platforms to include SDK headers only on
// Windows.  For win/ sources and headers, there's no difference; they're only
// compiled on Windows anyway.

#if defined(KAPPS_CORE_OS_WINDOWS)
#include "win/winapi.h"
#endif
