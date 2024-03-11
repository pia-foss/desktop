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

#include "uuid.h"
#include "stringslice.h"
#include <cassert>

namespace kapps { namespace core {

namespace
{
    // Take the lowest 4 bits from 'value', render them as a hex char, and
    // shift 'value' right 4 bits.
    char renderHexChar(std::uint64_t &value)
    {
        static const char hexChars[]{"0123456789abcdef"};
        std::uint64_t nextBits = value & 0xF;
        value >>= 4;
        return hexChars[nextBits];
    }
}

Uuid Uuid::buildV4(std::uint64_t randomHigh, std::uint64_t randomLow)
{
    // Set version to 4 (random)
    randomHigh &= 0xFFFFFFFFFFFF0FFF;
    randomHigh |= 0x0000000000004000;
    // Set variant to "Variant 1"
    randomLow &= 0x0FFFFFFFFFFFFFFF;
    randomLow |= 0x8000000000000000;
    return {randomHigh, randomLow};
}

#ifdef KAPPS_CORE_OS_WINDOWS
Uuid::Uuid(const GUID &guid)
{
    _val[0] = guid.Data1;   // 32 bits
    _val[0] <<= 16;
    _val[0] |= guid.Data2;  // 16 bits
    _val[0] <<= 16;
    _val[0] |= guid.Data3;  // 16 bits

    for(unsigned char byte : guid.Data4)
    {
        _val[1] <<= 8;  // No-op for first byte
        _val[1] |= byte;
    }
}
#endif

void Uuid::renderText(char *pBuffer) const
{
    assert(pBuffer);    // Ensured by caller
    std::uint64_t part{_val[1]};
    pBuffer[35] = renderHexChar(part);
    pBuffer[34] = renderHexChar(part);
    pBuffer[33] = renderHexChar(part);
    pBuffer[32] = renderHexChar(part);
    pBuffer[31] = renderHexChar(part);
    pBuffer[30] = renderHexChar(part);
    pBuffer[29] = renderHexChar(part);
    pBuffer[28] = renderHexChar(part);
    pBuffer[27] = renderHexChar(part);
    pBuffer[26] = renderHexChar(part);
    pBuffer[25] = renderHexChar(part);
    pBuffer[24] = renderHexChar(part);
    pBuffer[23] = '-';
    pBuffer[22] = renderHexChar(part);
    pBuffer[21] = renderHexChar(part);
    pBuffer[20] = renderHexChar(part);
    pBuffer[19] = renderHexChar(part);
    pBuffer[18] = '-';
    part = _val[0];
    pBuffer[17] = renderHexChar(part);
    pBuffer[16] = renderHexChar(part);
    pBuffer[15] = renderHexChar(part);
    pBuffer[14] = renderHexChar(part);
    pBuffer[13] = '-';
    pBuffer[12] = renderHexChar(part);
    pBuffer[11] = renderHexChar(part);
    pBuffer[10] = renderHexChar(part);
    pBuffer[ 9] = renderHexChar(part);
    pBuffer[ 8] = '-';
    pBuffer[ 7] = renderHexChar(part);
    pBuffer[ 6] = renderHexChar(part);
    pBuffer[ 5] = renderHexChar(part);
    pBuffer[ 4] = renderHexChar(part);
    pBuffer[ 3] = renderHexChar(part);
    pBuffer[ 2] = renderHexChar(part);
    pBuffer[ 1] = renderHexChar(part);
    pBuffer[ 0] = renderHexChar(part);
}

void Uuid::toString(char (&buffer)[37]) const
{
    buffer[36] = 0;
    renderText(buffer);
}

std::string Uuid::toString() const
{
    std::string result;
    result.resize(36);
    renderText(&result[0]);
    return result;
}

void Uuid::trace(std::ostream &os) const
{
    char uuidStr[37];
    toString(uuidStr);
    os << StringSlice{uuidStr};
}

}}
