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
#line SOURCE_FILE("win/inlines.cpp")

// Shared functionality between the Windows installer and daemon - see
// extras/installer/win/inlines.cpp.
//
// In the daemon, logging is performed with qInfo().

// PIA_PRODUCT_NAME is defined on the compiler command line in the installer,
// but it's in version.h in the daemon.
#include "version.h"

#define TAP_LOG qInfo
#include "../../../extras/installer/win/tap.inl"

#define SERVICE_LOG qInfo
#include "../../../extras/installer/win/service.inl"

#define TUN_LOG qInfo
#include "../../../extras/installer/win/tun.inl"
