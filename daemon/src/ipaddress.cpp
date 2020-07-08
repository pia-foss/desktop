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
#line SOURCE_FILE("ipaddress.cpp")

#include <array>

namespace
{
    // All IPv4 LAN and loopback subnets
    using SubnetPair = QPair<QHostAddress, int>;
    std::array<SubnetPair, 5> ipv4LocalSubnets{
        QHostAddress::parseSubnet(QStringLiteral("192.168.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("172.16.0.0/12")),
        QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/8")),
        QHostAddress::parseSubnet(QStringLiteral("169.254.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("127.0.0.0/8"))
    };

    // The modern infrastructure DNS range
    SubnetPair modernInfraDns{QHostAddress::parseSubnet(QStringLiteral("10.0.0.240/29"))};
}

bool isIpv4Local(const QHostAddress &addr)
{
    return std::any_of(ipv4LocalSubnets.begin(), ipv4LocalSubnets.end(),
        [&](const SubnetPair &subnet) {return addr.isInSubnet(subnet);});
}

bool isModernInfraDns(const QHostAddress &addr)
{
    return addr.isInSubnet(modernInfraDns);
}
