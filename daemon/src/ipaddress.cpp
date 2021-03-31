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
#include "ipaddress.h"
#include <array>

Ipv4Address::Ipv4Address(const QString &address)
    : Ipv4Address{}
{
    bool ipv4Ok{false};
    QHostAddress parsed{address};
    if(parsed.protocol() == QAbstractSocket::NetworkLayerProtocol::IPv4Protocol)
        _address = parsed.toIPv4Address(&ipv4Ok);
    if(!ipv4Ok)
    {
        qWarning() << "Invalid IPv4 address:" << address;
        _address = 0;
    }
}

bool Ipv4Address::inSubnet(quint32 netAddress, unsigned prefixLen) const
{
    // Convert the prefix length to a mask
    quint32 mask = 0;
    while(prefixLen)
    {
        mask >>= 1;
        mask |= 0x80000000u;
        --prefixLen;
    }
    return (_address & mask) == netAddress;
}

unsigned Ipv4Address::getSubnetMaskPrefix() const
{
    quint32 remainingBits = _address;
    unsigned prefixLength = 0;
    while(remainingBits & 0x80000000)
    {
        ++prefixLength;
        remainingBits <<= 1;
    }
    return prefixLength;
}

Ipv6Address::Ipv6Address(const AddressValue &address)
{
    std::copy(std::begin(address), std::end(address), std::begin(_address));
}

Ipv6Address::Ipv6Address(const QString &address)
    : Ipv6Address{}
{
    QHostAddress parsed{address};
    if(parsed.protocol() == QAbstractSocket::NetworkLayerProtocol::IPv6Protocol)
    {
        *this = {parsed.toIPv6Address().c};
    }
    else
    {
        qWarning() << "Invalid IPv6 address:" << address;
    }
}

bool Ipv6Address::operator==(const Ipv6Address &other) const
{
    return std::equal(std::begin(_address), std::end(_address),
                      std::begin(other._address));
}

bool Ipv6Address::operator<(const Ipv6Address &other) const
{
    return std::lexicographical_compare(std::begin(_address),
                                        std::end(_address),
                                        std::begin(other._address),
                                        std::end(other._address));
}

bool Ipv6Address::isLinkLocal() const
{
    return _address[0] == 0xfe && (_address[1] & 0xC0) == 0x80;
}
