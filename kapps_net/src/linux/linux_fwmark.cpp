// Copyright (c) 2024 Private Internet Access, Inc.
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

#include "linux_fwmark.h"
#include <sstream>

// Render a number as a hexadecimal string with "0x" - such as "0x1234",
// "0x123ABC", etc.  Does not include leading zeroes.
std::string hexNumberStr(unsigned value)
{
    std::stringstream s;
    s << "0x" << std::hex << value;
    return s.str();
}

namespace kapps { namespace net {

std::string Fwmark::excludePacketTag() const
{
    return hexNumberStr(_fwmarkBase);
}

std::string Fwmark::vpnOnlyPacketTag() const
{
    return hexNumberStr(_fwmarkBase + 1);
}

uint32_t Fwmark::wireguardFwmark() const
{
    return _fwmarkBase + 2;
}

std::string Fwmark::forwardedPacketTag() const
{
    return hexNumberStr(_fwmarkBase + 3);
}

}}
