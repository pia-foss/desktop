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
#line SOURCE_FILE("win/win_registry.cpp")

#include "win_registry.h"
#include "path.h"
#include "version.h"

#include <QDir>

#include <windows.h>
#pragma comment(lib, "advapi32.lib")

namespace {
    static const HKEY g_runKeyRoot = HKEY_CURRENT_USER;
    static const wchar_t* g_runKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static const wchar_t* g_runKeyName = L"" PIA_PRODUCT_NAME;
}

bool winLaunchAtLogin()
{
    return ERROR_SUCCESS == RegGetValueW(g_runKeyRoot, g_runKeyPath, g_runKeyName, RRF_RT_ANY, NULL, NULL, NULL);
}

void winSetLaunchAtLogin(bool enable)
{
    if (enable)
    {
        auto cmdline = QStringLiteral("\"%1\" --quiet").arg(QDir::toNativeSeparators(Path::ClientExecutable)).toStdWString();
        CHECK_ERROR(RegSetKeyValueW(g_runKeyRoot, g_runKeyPath, g_runKeyName, REG_SZ, cmdline.c_str(), cmdline.size() * 2));
    }
    else
        CHECK_ERROR_IF(error != ERROR_FILE_NOT_FOUND, RegDeleteKeyValueW(g_runKeyRoot, g_runKeyPath, g_runKeyName));
}
