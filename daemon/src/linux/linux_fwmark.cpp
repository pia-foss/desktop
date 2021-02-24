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
#line SOURCE_FILE("linux_fwmark.cpp")

#include "linux_fwmark.h"
#include "brand.h"

QString hexNumberStr(unsigned value)
{
    return QStringLiteral("0x") + QString::number(value, 16).toUpper();
}

namespace Fwmark
{
    const QString excludePacketTag{hexNumberStr(BRAND_LINUX_FWMARK_BASE)};
    const QString vpnOnlyPacketTag{hexNumberStr(BRAND_LINUX_FWMARK_BASE+1)};
    const uint32_t wireguardFwmark{BRAND_LINUX_FWMARK_BASE+2};
    const QString forwardedPacketTag{hexNumberStr(BRAND_LINUX_FWMARK_BASE+3)};
}
