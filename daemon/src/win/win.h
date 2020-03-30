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
#line HEADER_FILE("win/win.h")

#ifndef WIN_H
#define WIN_H
#pragma once

// Avoid including Windows and Winsock headers directly from other headers -
// they're sensitive to the exact order in which they're referenced, and they're
// sensitive to the NO* macros defined by builtin/common.h.  Instead, include
// win.h to get these headers in the exact order that works throughout the
// client.
//
// If you need APIs that are currently excluded by a NO* macro in
// builtin/common.h, just remove that macro there.  (Don't try to undefine it in
// your file and re-include Windows.h, etc.)

#undef _WINSOCKAPI_
#include <WinSock2.h>
#include <Windows.h>
#include <objbase.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <fwpvi.h>
#include <fwpmu.h>

#include <VersionHelpers.h>

#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "rpcrt4.lib")

#endif // WIN_H
