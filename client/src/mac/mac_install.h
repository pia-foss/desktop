// Copyright (c) 2019 London Trust Media Incorporated
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
#line HEADER_FILE("mac/mac_install.h")

#ifndef MAC_INSTALL_H
#define MAC_INSTALL_H
#pragma once

// Shared constants with install helper (only needed here to define Qt metadata,
// which has to be in the header for moc)
#include "mac_install_helper.h"

// Qt enums in MacInstall namespace - for tracing names mainly
namespace MacInstall
{
    Q_NAMESPACE
    Q_ENUM_NS(Action);
    Q_ENUM_NS(Result);
}

bool macCheckInstallation();

bool macExecuteInstaller();

bool macExecuteUninstaller();

#endif // MAC_INSTALL_H
