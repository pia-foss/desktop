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
#line HEADER_FILE("linux_fwmark.h")

#ifndef LINUX_FWMARK_H
#define LINUX_FWMARK_H

// Render a number as a hexadecimal string with "0x" - such as "0x1234",
// "0x123ABC", etc.  Does not include leading zeroes.
QString hexNumberStr(unsigned value);

// These are fwmark values used by PIA on Linux.  We use these for both iptables
// and Wireguard, so they're centralized here to ensure that we allocate values
// correctly.
namespace Fwmark
{
    extern const QString excludePacketTag;
    extern const QString vpnOnlyPacketTag;
    extern const uint32_t wireguardFwmark;
}

#endif
