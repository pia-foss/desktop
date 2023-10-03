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

#pragma once
#include "core.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// KACIPv4Address specifies an IPv4 address, which is simply a 32-bit integer.
// Addresses are always in host byte order.
typedef uint32_t KACIPv4Address;

// Many parts of the API express a "port array", such as the arrays of known
// service ports in regions and servers.  While C prevents us from defining a
// general "array slice" type for any value, port arrays are common enough to
// be worth a specific type.  This is also similar to KACStringSlice, but for
// port numbers.
typedef struct KACPortArray
{
    const uint16_t *data;   // May be nullptr if size is 0.
    size_t size;
} KACPortArray;

#ifdef __cplusplus
}
#endif
