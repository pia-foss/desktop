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

#ifndef IPADDRESS_H
#define IPADDRESS_H

#include <kapps_core/core.h>
#include <kapps_core/ipaddress.h>
#include "corejson.h"
#include "util.h"
#include "stringslice.h"

namespace kapps { namespace core {

// IPv4 address.  QHostAddress can hold an IPv4 address, but when we require an
// IPv4 address specifically, this makes more sense than holding a QHostAddress
// and expecting a specific type.
//
// The stored IPv4 address is in host byte order.
class KAPPS_CORE_EXPORT Ipv4Address : public OStreamInsertable<Ipv4Address>
{
public:
    // Default is 0.0.0.0
    Ipv4Address() : _address{} {}
    Ipv4Address(std::uint32_t address) : _address{address} {}
    // Create from each part (allows writing in decimal - e.g.
    // Ipv4Address{192, 168, 0, 0})
    Ipv4Address(std::uint8_t b0, std::uint8_t b1, std::uint8_t b2, std::uint8_t b3)
        : _address{(std::uint32_t{b0} << 24) | (std::uint32_t{b1} << 16) |
                   (std::uint32_t{b2} << 8) | std::uint32_t{b3}}
    {}
    // Parse an IP address from a string.  If it's not a valid IPv4 address,
    // the resulting Ipv4Address is 0.0.0.0.
    Ipv4Address(const std::string &address);

public:
    bool operator==(const Ipv4Address &other) const {return address() == other.address();}
    bool operator!=(const Ipv4Address &other) const {return !(*this == other);}
    bool operator<(const Ipv4Address &other) const {return address() < other.address();}

public:
    std::uint32_t address() const {return _address;}

    // Get a reference to the address.  This is rarely needed but is used to
    // implement the ArraySlice<Ipv4Address> -> KACArraySlice (of KACIPv4Address)
    // conversion.  (We could friend it so it could do &addr._address itself,
    // but then there'd still be a public way to get this address, so just
    // provide it the simple way.)
    //
    // This must always return a reference to _address - it can't conditionally
    // return the address of a constant, etc.
    const std::uint32_t &addressRef() const {return _address;}

    std::string toString() const;
    bool isNull() const {return _address == 0;}

    // Test if the address is in a particular subnet.  For example to test if it
    // is 10/8:
    //   addr.inSubnet(0x0A000000, 8)
    bool inSubnet(std::uint32_t netAddress, unsigned prefixLen) const;

    // Check for specific IPv4 special address blocks.  A variety of groups are
    // provided.
    //
    // Frequently, these are used to determine whether we should treat an
    // address as reachable through the VPN or local (such as LAN DNS servers,
    // etc.)  Note that 10.0.0.240/29 is a private use block but is used for
    // PIA's VPN DNS, and other 10/8 addresses are used internally for various
    // reasons (such as the meta service).  These tests are carefully named to
    // clarify precisely what they test - e.g. "private use blocks" vs. "LAN",
    // because some 10/8 addresses are private use but are not considered "LAN"
    // by PIA.

    // Test if the address is a loopback address (127/8)
    bool isLoopback() const {return inSubnet(0x7F000000, 8);}

    // Test if the address is in the PIA DNS block (10.0.0.240/29)
    bool isPiaDns() const {return inSubnet(0x0A0000F0, 29);}

    // Test if an address is in an IPv4 private use block
    bool isPrivateUse() const
    {
        return inSubnet(0xC0A80000, 16) ||  // 192.168/16
               inSubnet(0xAC100000, 12) ||  // 172.16/12
               inSubnet(0x0A000000, 8) ||   // 10/8
               inSubnet(0xA9FE0000, 16);    // 169.254/16
    }

    // 224/4
    bool isMulticast() const {return inSubnet(0xE0000000, 4);}

    // 255.255.255.255/32
    bool isBroadcast() const {return inSubnet(0xFFFFFFFF, 32);}

    // Test if an address for a DNS server should be routed locally (not through
    // the VPN).  Note that this is specific to DNS (and specific to PIA's DNS
    // address allocation); other 10/8 addresses may be meaningful on the VPN in
    // other contexts (like meta services).
    bool isLocalDNS() const {return !isPiaDns() && (isLoopback() || isPrivateUse());}

    // Get a prefix length from an IPv4 subnet mask (counts consecutive leading
    // 1 bits - i.e. 255.255.128.0 -> 17)
    unsigned getSubnetMaskPrefix() const;

    void trace(std::ostream &os) const {os << toString();}

    static Ipv4Address maskIpv4(const Ipv4Address &address, unsigned prefix);
    static Ipv4Address maskIpv4(std::uint32_t address, unsigned prefix);

private:
    std::uint32_t _address;
};

// Ipv4Address can be converted to/from JSON.  A string containing a nonzero
// IP address is expected - note that 0.0.0.0 is _not_ valid.
template<class JsonT>
void to_json(JsonT &j, const Ipv4Address &v)
{
    j = v.toString();
}

template<class JsonT>
void from_json(const JsonT &j, Ipv4Address &v)
{
    Ipv4Address addr = j.template get_ref<const std::string&>();
    // Invalid addresses or 0.0.0.0 are not accepted
    if(addr == Ipv4Address{})
    {
        KAPPS_CORE_WARNING() << "Expected IPv4 address, got value" << j;
        throw std::runtime_error{"Invalid IPv4 address"};
    }
    v = std::move(addr);
}

// IPv6 address.  Like Ipv4Address, this makes more sense than holding a
// QHostAddress and expecting a specific type.
//
// The stored IPv6 address is in network byte order (unlike Ipv4Address).
class KAPPS_CORE_EXPORT Ipv6Address : public OStreamInsertable<Ipv6Address>
{
public:
    using AddressValue = std::uint8_t[16];

private:
    // High or low 8 bits of a uint16_t, used by the uint16_t w0-w7 constructor
    static std::uint8_t h8(std::uint16_t w) {return static_cast<std::uint8_t>(w >> 8);}
    static std::uint8_t l8(std::uint16_t w) {return static_cast<std::uint8_t>(w);}

public:
    // Default is ::
    Ipv6Address() : _address{} {}
    Ipv6Address(const std::uint8_t* address);
    // Create an Ipv6Address from a literal value.  This is intended to simplify
    // literal hard-coded IPv6 addresses, such as reserved ranges.  Dynamic
    // addresses should usually use the const std::uint8_t* constructor.
    //
    // For example:
    // Ipv6Address{0x2001,0x0db8,0x0000,0x0000,0x0000,0x0000,0x1234,0x5678}
    // == 2001:db8::1234:5678
    // == Ipv6Address{0x2001, 0xdb8, 0, 0, 0, 0, 0x1234, 0x5678}
    //
    // Note that leading zeroes in each 16-bit group can be abbreviated like
    // text addresses.  There's no way to abbreviate an entire middle section
    // like '::', but any parameters not specified default to 0, so
    // Ipv6Address{0xfc00} represents "fc00::".
    Ipv6Address(std::uint16_t w0, std::uint16_t w1=0, std::uint16_t w2=0,
                std::uint16_t w3=0, std::uint16_t w4=0, std::uint16_t w5=0,
                std::uint16_t w6=0, std::uint16_t w7=0)
        : _address{h8(w0), l8(w0), h8(w1), l8(w1), h8(w2), l8(w2),
                   h8(w3), l8(w3), h8(w4), l8(w4), h8(w5), l8(w5),
                   h8(w6), l8(w6), h8(w7), l8(w7)}
    {}
    // Parse an IP address from a string.  If it's not a valid IPv6 address,
    // the resulting Ipv6Address is ::.
    Ipv6Address(const std::string &address);

public:
    bool operator==(const Ipv6Address &other) const;
    bool operator!=(const Ipv6Address &other) const {return !(*this == other);}
    bool operator<(const Ipv6Address &other) const;

public:
    bool isNull() const;
    const AddressValue &address() const {return _address;}
    std::string toString() const;

    // Test if the address is an IPv6 link-local address
    // fe80::/10
    bool isLinkLocal() const;
    // ::1/128
    bool isLoopback() const;
    // fc00::/7
    bool isUniqueLocalUnicast() const;
    // ff00::/8
    bool isMulticast() const;

    bool inSubnet(const AddressValue &addr, std::uint32_t prefixLength) const;
    bool inSubnet(const Ipv6Address &ipAddr, std::uint32_t prefixLength) const;

    static Ipv6Address maskIpv6(const Ipv6Address::AddressValue &address, unsigned prefix);
    static Ipv6Address maskIpv6(const Ipv6Address &address, unsigned prefix);

    void trace(std::ostream &os) const {os << toString();}

private:
    AddressValue _address;
};

// An IPv4 subnet - combination of an IPv4 address and a CIDR prefix length.
class KAPPS_CORE_EXPORT Ipv4Subnet : public OStreamInsertable<Ipv4Subnet>
{
public:
    // The default is 0.0.0.0/0.
    Ipv4Subnet() : _address{}, _prefix{} {}
    // Create from an IPv4 address and prefix length.  The prefix length must
    // be in range [0, 32]; otherwise a std::runtime_error is thrown.  There
    // are no requirements on the address (in particular, bits after the prefix
    // can be nonzero; they are cleared by Ipv4Subnet).
    Ipv4Subnet(Ipv4Address address, unsigned prefix);
    // Parse from a string.  The most common subnet format is
    // "<IPv4_addr>/<prefix>".
    //
    // Additionally:
    //  - Up to 3 trailing octets of the IPv4 address can be omitted, in which
    //    case they default to 0.  The trailing dot is optional if an octet has
    //    been omitted.
    //  - "/<prefix>" can be omitted, in which case it defaults to the length
    //    of the octets specified (8, 16, 24, or 32)
    //
    // Because of the above, this is _not_ equivalent to splitting up the
    // string and parsing with Ipv4Address, as it does not accept omitted
    // octets.
    //
    // So, all of the following are accepted and equivalent:
    //  - 10
    //  - 10.
    //  - 10/8
    //  - 10./8
    //  - 10.0/8
    //  - 10.0./8
    //  - 10.0.0/8
    //  - 10.0.0./8
    //  - 10.0.0.0/8
    //  - 10.0.0.1/8 (trailing bits are cleared)
    //
    // The following are accepted but not equivalent to the above:
    //  - 10.0. (== 10.0.0.0/16)
    //  - 10.0.0.1 (== 10.0.0.1/32)
    //
    // Finally, note that an "IPv4-style" netmask is _not_ accepted (unlike
    // QHostAddress); only a CIDR prefix length is accepted.  (E.g.
    // "10.0.0.0/255.0.0.0" is not valid, use "10.0.0.0/8" instead.)
    Ipv4Subnet(StringSlice subnet);

public:
    bool operator==(const Ipv4Subnet &other) const;

    void trace(std::ostream &os) const;

    const Ipv4Address &address() const {return _address;}
    unsigned prefix() const {return _prefix;}

    void address(const Ipv4Address &address) {_address = address;}
    void prefix(unsigned prefix) {_prefix = prefix;}

private:
    Ipv4Address _address;
    unsigned _prefix;
};

// An IPv6 subnet - combination of an IPv6 address and a CIDR prefix length.
class KAPPS_CORE_EXPORT Ipv6Subnet : public OStreamInsertable<Ipv4Subnet>
{
public:
    // The default is ::/0.
    Ipv6Subnet() : _address{}, _prefix{} {}
    // Create from an IPv6 address and prefix length.  The prefix length must
    // be in range [0, 128]; otherwise a std::runtime_error is thrown.  There
    // are no requirements on the address (in particular, bits after the prefix
    // length are _not_ forced to 0).
    Ipv6Subnet(Ipv6Address address, unsigned prefix);
    // Parse from a string.  This accepts a subnet in the form
    // "<IPv6_address>/<prefix>".
    //
    // Because IPv6 addresses already support abbreviations ("::"), there is no
    // additional abbreviation support in Ipv6Subnet, unlike Ipv4Subnet.  The
    // "/<prefix>" can be omitted, in which case the prefix always defaults to
    // 128, unlike Ipv4Address.
    //
    // Parsing with Ipv6Subnet is therefore essentially the same as splitting
    // and parsing with Ipv6Address.
    Ipv6Subnet(StringSlice subnet);

public:
    bool operator==(const Ipv6Subnet &other) const;

    void trace(std::ostream &os) const;

    const Ipv6Address &address() const {return _address;}
    unsigned prefix() const {return _prefix;}

    void address(const Ipv6Address &address) {_address = address;}
    void prefix(unsigned prefix) {_prefix = prefix;}

private:
    Ipv6Address _address;
    unsigned _prefix;
};

// Ipv4Address and ArraySlice<Ipv4Address> can be adapted to KACIPv4Address and
// KACArraySlice (of KACIPv4Address values).
//
// KACIPv4Address is just a uint32_t, so the array slice conversion is
// specialized to return the address of Ipv4Address::_address with stride
// sizeof(Ipv4Address).  (At the moment, this is the same as the default since
// Ipv4Address::_address is its sole data member, but this is robust even if
// Ipv4Address would gain another member, a vtable, etc., even through a base
// class.
inline KAPPS_CORE_EXPORT KACIPv4Address toApi(const Ipv4Address &addr) {return addr.address();}
inline KAPPS_CORE_EXPORT Ipv4Address fromApi(KACIPv4Address addr) {return {addr};}
KAPPS_CORE_EXPORT KACArraySlice toApi(ArraySlice<const Ipv4Address> addrs);
// Adapt a KACPortArray to ArraySlice<const std::uint16_t>
inline ArraySlice<const std::uint16_t> fromApi(KACPortArray ports)
{
    return {ports.data, ports.size};
};

}}

#endif
