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

#include "mac_version.h"
#import <Foundation/NSProcessInfo.h>

namespace kapps { namespace core {

std::tuple<unsigned, unsigned, unsigned> currentMacosVersion()
{
    auto osVersion = [NSProcessInfo processInfo].operatingSystemVersion;

    std::tuple<unsigned, unsigned, unsigned> result{};
    // The NSOperatingSystemVersion members are signed - surely they'll never be
    // negative, but clamp to 0 if they somehow are.
    if(osVersion.majorVersion >= 0)
        std::get<0>(result) = static_cast<unsigned>(osVersion.majorVersion);
    if(osVersion.majorVersion >= 0)
        std::get<1>(result) = static_cast<unsigned>(osVersion.minorVersion);
    if(osVersion.majorVersion >= 0)
        std::get<2>(result) = static_cast<unsigned>(osVersion.patchVersion);
    return result;
}

}}
