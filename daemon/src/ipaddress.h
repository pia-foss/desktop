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

#ifndef IPADDRESS_H
#define IPADDRESS_H

#include "common.h"

// Test if an address is an IPv4 LAN or loopback address
bool isIpv4Local(const QHostAddress &addr);
// Test if an address is in the modern infrastructure DNS range
bool isModernInfraDns(const QHostAddress &addr);

// IPv4 address.  QHostAddress can hold an IPv4 address, but when we require an
// IPv4 address specifically, this makes more sense than holding a QHostAddress
// and expecting a specific type.
//
// The stored IPv4 address is in host byte order.
class Ipv4Address : public DebugTraceable<Ipv4Address>
{
public:
    // Default is 0.0.0.0
    Ipv4Address() : _address{} {}
    Ipv4Address(quint32 address) : _address{address} {}
    // Parse an IP address from a string.  If it's not a valid IPv4 address,
    // the resulting Ipv4Address is 0.0.0.0.
    Ipv4Address(const QString &address);

public:
    bool operator==(const Ipv4Address &other) const {return address() == other.address();}
    bool operator!=(const Ipv4Address &other) const {return !(*this == other);}
    bool operator<(const Ipv4Address &other) const {return address() < other.address();}

public:
    quint32 address() const {return _address;}
    QString toString() const {return QHostAddress{_address}.toString();}
    operator QHostAddress() const {return QHostAddress{_address};}

    // Test if the address is in a particular subnet.  For example to test if it
    // is 10/8:
    //   addr.inSubnet(0x0A000000, 8)
    bool inSubnet(quint32 netAddress, unsigned prefixLen) const;

    // Test if the address is a loopback address (127/8)
    bool isLoopback() const {return inSubnet(0x7F000000, 8);}

    // Get a prefix length from an IPv4 subnet mask (counts consecutive leading
    // 1 bits - i.e. 255.255.128.0 -> 17)
    unsigned getSubnetMaskPrefix() const;

    void trace(QDebug &dbg) const {dbg << toString();}

private:
    quint32 _address;
};

// IPv6 address.  Like Ipv4Address, this makes more sense than holding a
// QHostAddress and expecting a specific type.
//
// The stored IPv6 address is in network byte order (unlike Ipv4Address).
class Ipv6Address : public DebugTraceable<Ipv6Address>
{
public:
    using AddressValue = quint8[16];
public:
    // Default is ::
    Ipv6Address() : _address{} {}
    Ipv6Address(const AddressValue &address);
    // Parse an IP address from a string.  If it's not a valid IPv6 address,
    // the resulting Ipv6Address is ::.
    Ipv6Address(const QString &address);

public:
    bool operator==(const Ipv6Address &other) const;
    bool operator!=(const Ipv6Address &other) const {return !(*this == other);}
    bool operator<(const Ipv6Address &other) const;

public:
    const AddressValue &address() const {return _address;}
    QString toString() const {return QHostAddress{_address}.toString();}
    operator QHostAddress() const {return QHostAddress{_address};}

    // Test if the address is an IPv6 link-local address - fe80::/10
    bool isLinkLocal() const;

    void trace(QDebug &dbg) const {dbg << toString();}

private:
    AddressValue _address;
};

#endif
