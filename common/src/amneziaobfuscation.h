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

#pragma once
#include "common.h"

#include <cstdint>
#include <optional>

// @brief Value object representing Amnezia VPN obfuscation parameters
//
// These are the configuration parameters used for Amnezia VPN's traffic
// obfuscation mechanism.  They are provided by the server when authenticating
// with an AmneziaWG endpoint and applied to the userspace WireGuard driver.
struct COMMON_EXPORT AmneziaObfuscation
{
    uint64_t initSeed1{};
    uint64_t initSeed2{};
    uint64_t junkCount{};
    uint64_t junkMinSize{};
    uint64_t junkMaxSize{};
    uint64_t headerChecksum1{};
    uint64_t headerChecksum2{};
    uint64_t headerChecksum3{};
    uint64_t headerChecksum4{};
};
