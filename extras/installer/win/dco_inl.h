// Copyright (c) 2026 Private Internet Access, Inc.
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

#pragma once
#include "common_inl.h" // DriverStatus
#include "service_inl.h"
#include <Windows.h>
#include <string>

// This must match the DCO driver's INF filename and the device hardware ID
// declared inside it.  It is not derived from BRAND_PREFIX because it is fixed
// by the signed driver payload in deps/dco - renaming the driver requires
// re-signing it and updating the INF to match.
#define DCO_DEVICE_NAME L"pia-ovpn-dco"

// Whether this is Windows 11 (build 22000+) - selects the win10/win11 DCO
// driver payload subdirectory.
bool IsWindows11();

// Whether a DCO device with the DCO_DEVICE_NAME hardware ID is present.
//
// tapinstall "remove" exits 0 whether or not it matched a device, so this is
// the only way to tell a clean install from a driver replacement - which
// decides whether an aborted installation has to remove the driver again or
// put the previous one back.
bool dcoDeviceInstalled();

DriverStatus installDcoDriver(LPCWSTR inf, LPCWSTR deviceName, LPCWSTR tapInstallPath);
DriverStatus uninstallDcoDriver(LPCWSTR inf, LPCWSTR deviceName, LPCWSTR tapInstallPath);

// Map a tapinstall.exe exit code to a DriverStatus.
// 1 = install, 2 = uninstall
DriverStatus mapExitCodeToDriverStatus(int action, int exitCode);
