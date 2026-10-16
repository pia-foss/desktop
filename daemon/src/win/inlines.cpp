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

#include <common/src/common.h>
#line SOURCE_FILE("win/inlines.cpp")

// Shared functionality between the Windows installer and daemon - see
// extras/installer/win/inlines.cpp.
//
// In the daemon, logging is performed with qInfo().

// PIA_PRODUCT_NAME is defined on the compiler command line in the installer,
// but it's in product.h in the daemon.
#include "product.h"

#include "../../../extras/installer/win/common_inl.h"

// callout.inl defines the setupapi helpers shared with the TAP and DCO code, so
// it has to precede them.
#define CALLOUT_LOG(...) \
do { \
    char buffer[1024]; \
    snprintf(buffer, sizeof(buffer), __VA_ARGS__); \
    qInfo() << buffer; \
} while(0)
#include "../../../extras/installer/win/callout.inl"

#define TAP_LOG(...) \
do { \
    char buffer[1024]; \
    snprintf(buffer, sizeof(buffer), __VA_ARGS__); \
    qInfo() << buffer; \
} while(0)
#include "../../../extras/installer/win/tap.inl"

#define DCO_LOG qInfo
#include "../../../extras/installer/win/dco.inl"

#define SERVICE_LOG qInfo
#include "../../../extras/installer/win/service.inl"

#define TUN_LOG qInfo
#include "../../../extras/installer/win/tun.inl"

#include "../../../extras/installer/win/util.inl"
