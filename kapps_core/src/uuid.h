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

#include "util.h"
#include <kapps_core/core.h>
#include <array>

#include "winapi.h" // For GUID constructor of Uuid

namespace kapps { namespace core {

// Basic UUID class - can generate a V4 (random) UUID and render UUIDs to
// strings.
class KAPPS_CORE_EXPORT Uuid : public OStreamInsertable<Uuid>
{
public:
    // Generate a V4 (random) UUID from random data.  The data should be
    // generated with a cryptographically secure random number generator
    static Uuid buildV4(std::uint64_t randomHigh, std::uint64_t randomLow);

public:
    // Create a zero Uuid
    Uuid() : _val{} {}

    // Create a Uuid from 128-bit data (native endian)
    Uuid(std::uint64_t high, std::uint64_t low) : _val{high, low} {}

#ifdef KAPPS_CORE_OS_WINDOWS
    // Create a Uuid from a Windows GUID
    Uuid(const GUID &guid);
#endif

private:
    // Render the UUID into a 36-char buffer
    void renderText(char *pBuffer) const;

public:
    // Render the UUID as a string
    void toString(char (&buffer)[37]) const;
    std::string toString() const;

    void trace(std::ostream &os) const;

private:
    // 128-bit UUID value (native endian)
    std::array<std::uint64_t, 2> _val;
};

}}
