// Copyright (c) 2023 Private Internet Access, Inc.
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
#line HEADER_FILE("win/win_registry.h")

#ifndef WIN_REGISTRY_H
#define WIN_REGISTRY_H
#pragma once

// These functions control whether the client launches at login on Windows.
// If the operation can't be completed, these will throw an Error.

// Test if the client is currently configured to launch at login.
bool winLaunchAtLogin();

// Enable or disable launch at startup.
void winSetLaunchAtLogin(bool enable);

#endif // WIN_REGISTRY_H
