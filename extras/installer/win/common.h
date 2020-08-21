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

#ifndef COMMON_H
#define COMMON_H
#pragma once

#define NOMINMAX    // Suppress annoying min / max macros from windows.h

//#define OEMRESOURCE
#define STRICT 1
#include <windows.h>
//#include <windowsx.h>
#include <versionhelpers.h>

// This undocumented message tells a window to display its system menu
#define WM_POPUPSYSTEMMENU 0x0313
// Other undocumented messages
#define WM_UAHDESTROYWINDOW 0x0090
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092
#define WM_UAHINITMENU 0x0093
#define WM_UAHMEASUREMENUITEM 0x0094
#define WM_UAHNCPAINTMENUPOPUP 0x0095

#include "resource.h"

#include <shlwapi.h>
#include <tchar.h>
#include <wchar.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef UNICODE
#error Non-unicode builds not supported
#endif

extern bool g_silent;
extern bool g_passive;

extern HINSTANCE g_instance;

extern std::wstring g_executablePath;
extern std::wstring g_installPath;
extern std::wstring g_userTempPath;
extern std::wstring g_systemTempPath;
extern std::wstring g_startMenuPath;
extern std::wstring g_clientPath;
extern std::wstring g_servicePath;
extern std::wstring g_wgServicePath;
extern std::wstring g_clientDataPath;
extern std::wstring g_daemonDataPath;
extern std::wstring g_oldDaemonDataPath;

// Must be at end of file
#include "util.h"

#endif // COMMON_H
