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

#ifndef TUN_INL_H
#define TUN_INL_H

#include <string>
#include <vector>

// Initialize MSI library logging and UI settings.  This is global state in the
// MSI library, so it applies to everything using that library.
void initMsiLib(const wchar_t *pLoggingDir);

// Find all installed products matching the brand's WinTUN package.
std::vector<std::wstring> findInstalledWintunProducts();

// Uninstall an MSI product.
bool uninstallMsiProduct(const wchar_t *pProductCode);

// Install an MSI package from a .msi file.
bool installMsiPackage(const wchar_t *pPackagePath);

bool isWintunSupported();

#endif
