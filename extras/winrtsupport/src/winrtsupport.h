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
#line HEADER_FILE("winrtsupport.h")

#ifndef WINRTSUPPORT_H
#define WINRTSUPPORT_H

#include <QString>
#include <vector>
#include <cstddef>

// This header defines structures used to interface with pia-winrtsupport.dll.
// It's included by the client and daemon, so it must not reference any WinRT
// types.

// pia-winrtsupport.dll is loaded by the client and daemon at runtime (only if
// the OS supports it) to provide access to Windows Runtime APIs (currently, to
// enumerate and inspect UWP apps for the Split Tunnel feature).
//
// Windows Runtime APIs are spread among various DLLs, and much of the C++
// projection is actually templated.  Dynamically loading the Windows Runtime
// API would require using the raw ABI interfaces and would still be difficult.
//
// Instead this module acts as a shim around the basic API operations we need
// that can be loaded dynamically.

// App entry returned by getUwpApps().
//
// UWP actually makes distinctions among all of the following:
// - an app (something the user can run, appears in Start Menu)
// - a package (group of files that can be installed, can contain 0 or more
//   apps, versions are distinct)
// - a package family (group including all versioned packages)
//
// Split tunnel app rules in PIA identify a "package family" only and apply to
// all installed packages in that family.
//
// To display a package family, we use the display name and log of the first
// installed app in any installed package from that family.  This isn't
// technically correct, but:
// - packages never seem to have display names or logos (despite the fact that
//   there are API elements for them)
// - it's (intuitively) rare for a package to actually contain more than 1 app,
//   so any "family"/"app" distinction would probably be more confusing in the
//   most common case
struct EnumeratedUwpApp
{
    QString displayName;
    QString appPackageFamily;
};

// Since we really want these functions to have C++ linkage, runtime linking is
// performed by returning a function pointer table.  This is more robust than
// using GetProcAddress() on each individual entry point.
struct WinRtSupportEntryPoints
{
    void(*pInitWinRt)();
    std::vector<EnumeratedUwpApp>(*pGetUwpApps)();
    QString(*pLoadAppDisplayName)(const QString&);
    std::vector<std::uint8_t>(*pLoadAppIcon)(const QString&, float, float);
    std::vector<QString>(*pAdminGetInstallDirs)(const QString&);
};

#endif
