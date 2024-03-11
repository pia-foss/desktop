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

#include "ipaddress.h"
#include <array>
#include <string>
#include <algorithm>

#if defined(KAPPS_CORE_OS_POSIX)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#elif defined(KAPPS_CORE_OS_WINDOWS)
#include "winapi.h"
#pragma comment(lib, "ws2_32.lib")
#endif

#include <iostream>

namespace kapps { namespace core {

namespace
{
    // Break up a subnet string into a StringSlice for the IP address section,
    // and a prefix length.  The prefix length is parsed - if it's present but
    // not a valid nonnegative integer, a std::runtime_error is thrown.
    //
    // If the prefix length is omitted, -1 is returned for the prefix.
    std::pair<StringSlice, int> breakSubnet(StringSlice subnet)
    {
        auto slashPos = subnet.find('/');
        if(slashPos == StringSlice::npos)
            return {subnet, -1};    // Subnet was omitted

        // slashPos+1 is in range even if the slash was the last char, because
        // substr(size()) is valid and returns an empty substring.
        auto prefix = parseInteger<int>(subnet.substr(slashPos+1));
        if(prefix < 0)
            throw std::runtime_error{"negative subnet prefix name not allowed"};

        return {subnet.substr(0, slashPos), prefix};
    }
}

Ipv4Address::Ipv4Address(const std::string &addressString)
    : Ipv4Address{}
{
    in_addr networkAddress{};
    if (inet_pton(AF_INET, addressString.c_str(), &networkAddress) == 1)
        // We store in host byte order
        _address = ntohl(networkAddress.s_addr);
}

bool Ipv4Address::inSubnet(std::uint32_t netAddress, unsigned prefixLen) const
{
    // Convert the prefix length to a mask
    std::uint32_t mask = 0;
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
    std::uint32_t remainingBits = _address;
    unsigned prefixLength = 0;
    while(remainingBits & 0x80000000)
    {
        ++prefixLength;
        remainingBits <<= 1;
    }
    return prefixLength;
}

std::string Ipv4Address::toString() const
{
    static const std::uint32_t inet4AddrStrLen{16};
    char buf[inet4AddrStrLen]{};
    // Convert from host to network
    std::uint32_t networkAddress{htonl(_address)};
    if(inet_ntop(AF_INET, &networkAddress, buf, sizeof(buf)))
        return buf;
    else
        return {};
}

Ipv4Address Ipv4Address::maskIpv4(const Ipv4Address &address, unsigned prefix)
{
    return maskIpv4(address.address(), prefix);
}

Ipv4Address Ipv4Address::maskIpv4(std::uint32_t address, unsigned prefix)
{
    // Need to special-case prefix of 0
    // as a << of 32 results in undefined behavior for a uint32_t
    if(prefix == 0)
        return {};

    prefix = std::min(prefix, 32u);
    std::uint32_t maskBits = ~std::uint32_t{0} << (32 - prefix);
    return {address & maskBits};
}

Ipv6Address::Ipv6Address(const uint8_t *address)
{
    ::memcpy(_address, address, sizeof(_address));
}

Ipv6Address::Ipv6Address(const std::string &addressString)
    : Ipv6Address{}
{
    in6_addr networkAddress{};
    if(inet_pton(AF_INET6, addressString.c_str(), &networkAddress) == 1)
    {
        std::copy(&networkAddress.s6_addr[0], &networkAddress.s6_addr[16],
                  &_address[0]);
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

bool Ipv6Address::isNull() const
{
    return std::all_of(std::begin(_address), std::end(_address), [](std::uint8_t c)
    {
        return c == 0;
    });
}

// fe80::/10
bool Ipv6Address::isLinkLocal() const
{
    return _address[0] == 0xfe && (_address[1] & 0xc0) == 0x80;
}

bool Ipv6Address::isLoopback() const
{
    // ::1/128
    bool result = std::all_of(std::begin(_address), std::end(_address) - 1, [](std::uint8_t c)
    {
        return c == 0;
    });
    return (result && _address[15] == 1);
}

bool Ipv6Address::isUniqueLocalUnicast() const
{
    // fc00::/7
    return (_address[0] & 0xfc) == 0xfc;
}

bool Ipv6Address::isMulticast() const
{
    // ff00::/8
    return _address[0] == 0xff;
}

bool Ipv6Address::inSubnet(const AddressValue &addr, std::uint32_t prefixLength) const
{
    prefixLength = std::min(prefixLength, 128u);

    // Special-case -- all bits must be identical
    if(prefixLength == 128)
        return std::equal(std::begin(_address), std::end(_address), std::begin(addr));

    // Invalid prefix length (only 128 bits in ipv6 address)
    else if(prefixLength > 128)
        return false;

    int index = prefixLength / 8; // Index of possible partial byte
    // Subnet is on a byte boundary
    // so we can do a simple comparison of bytes
    if(prefixLength % 8 == 0)
        return std::equal(_address, _address + index, addr);

    // If prefixLength is not on a byte boundary
    // then we have to examine a partial byte.
    // Calculate how many bits of the partial byte we care about
    // given a prefixLength of (for example) 34. To do this
    // divide prefixLength by 8 to find how many
    // whole bytes go into it (4), multiply this by 8
    // to find the number of bits in the nearest
    // whole byte: 4 * 8 == 32. Then subtract this from
    // the prefixLength: 34 - 32 == 2. So we need to look
    // at the first 2 bits of the fifth byte.
    unsigned bytePrefix = prefixLength - (index * 8);

    // If we wish to look at the first (for ex.) 2 bits of a byte
    // we need a mask with only the 2 leftmost bits set to 1
    // and every other bit 0. To do this, we start with 0xFF
    // which is 11111111 in binary - and then shift it 6 bits (8 - bytePrefix)
    // to the left, this gives us: 11000000. Bitwise &-ing this with
    // another byte will preseve only the the leftmost 2 bits of
    // that byte.
    uint8_t maskBits = 0xFF << (8 - bytePrefix);

    // If the partial byte doesn't match, then our address is not
    // part of the subnet.
    if((_address[index] & maskBits) != (addr[index] & maskBits))
        return false;

    // If it does match, it may be part of the subnet
    // but verify every prior byte also matches.
    return std::equal(std::begin(_address), _address + index, std::begin(addr));
}

bool Ipv6Address::inSubnet(const Ipv6Address &ipAddr, std::uint32_t prefixLength) const
{
    return inSubnet(ipAddr._address, prefixLength);
}

std::string Ipv6Address::toString() const
{
    // Maximum size of ipv6 address string
    static const std::uint32_t inet6AddrStrLen{46};
    char buf[inet6AddrStrLen]{};
    if(inet_ntop(AF_INET6, _address, buf, sizeof(buf)))
        return buf;
    else
        return {};
}

Ipv6Address Ipv6Address::maskIpv6(const Ipv6Address &address, unsigned prefix)
{
    return maskIpv6(address.address(), prefix);
}

Ipv6Address Ipv6Address::maskIpv6(const Ipv6Address::AddressValue &address, unsigned prefix)
{
    if(prefix >= 128)
        return {address};

    Ipv6Address::AddressValue addressMasked;
    std::copy(&address[0], &address[16], &addressMasked[0]);

    prefix = std::min(prefix, 128u);
    int i=prefix/8; // Index of possible partial byte
    prefix -= i*8;  // Number of bits to keep in the next byte - [0, 7]

    std::uint8_t maskBits = ~std::uint8_t{0} << (8 - prefix);
    addressMasked[i] &= maskBits;
    ++i;

    // Zero all remaining bytes
    while(i < 16)
    {
        addressMasked[i] = 0;
        ++i;
    }

    return addressMasked;
}

Ipv4Subnet::Ipv4Subnet(Ipv4Address address, unsigned prefix)
    : _address{Ipv4Address::maskIpv4(address.address(), prefix)}, _prefix{prefix}
{
}

Ipv4Subnet::Ipv4Subnet(StringSlice subnet)
{
    auto brokenSubnet = breakSubnet(subnet);

    // To handle omitted octets in IPv4, just check how many octets seem to be
    // present based on dots, and append '.0.0.0', '.0.0', or '.0'.
    int dots{0};
    for(char c : brokenSubnet.first)
    {
        if(c == '.')
            ++dots;
    }

    // More than three dots is never OK (don't allow '127.0.0.1.')
    if(dots > 3)
        throw std::runtime_error{"invalid IPv4 address in subnet"};

    // A trailing dot is OK otherwise
    if(brokenSubnet.first.ends_with('.'))
    {
        brokenSubnet.first = brokenSubnet.first.substr(0, brokenSubnet.first.size()-1);
        --dots;
    }

    // We need a null-terminated string for inet_pton(), and we might need to
    // append a suffix
    auto addressPart = brokenSubnet.first.to_string();
    int defaultPrefix = 0;

    switch(dots)
    {
        case 0: // 10
            addressPart += ".0.0.0";
            defaultPrefix = 8;
            break;
        case 1: // 10.0
            addressPart += ".0.0";
            defaultPrefix = 16;
            break;
        case 2: // 10.0.0
            addressPart += ".0";
            defaultPrefix = 24;
            break;
        default:
        case 3: // 10.0.0.1
            defaultPrefix = 32;
            break;
    }

    in_addr parsedAddr{};
    if(inet_pton(AF_INET, addressPart.c_str(), &parsedAddr) != 1)
        throw std::runtime_error{"invalid IPv4 address in subnet"};

    if(brokenSubnet.second < 0)
        brokenSubnet.second = defaultPrefix;
    else if(brokenSubnet.second > 32)
        throw std::runtime_error{"invalid prefix length in IPv4 subnet"};

    _prefix = static_cast<unsigned>(brokenSubnet.second);
    _address = Ipv4Address::maskIpv4(ntohl(parsedAddr.s_addr), _prefix);
}

bool Ipv4Subnet::operator==(const Ipv4Subnet &other) const
{
    return _address == other._address && _prefix == other._prefix;
}

void Ipv4Subnet::trace(std::ostream &os) const
{
    os << _address << '/' << _prefix;
}

Ipv6Subnet::Ipv6Subnet(Ipv6Address address, unsigned prefix)
    : _address{Ipv6Address::maskIpv6(address.address(), prefix)}, _prefix{prefix}
{
}

Ipv6Subnet::Ipv6Subnet(StringSlice subnet)
{
    auto brokenSubnet = breakSubnet(subnet);

    if(brokenSubnet.second < 0)
        brokenSubnet.second = 128;
    else if(brokenSubnet.second >= 128)
        throw std::runtime_error{"invalid prefix length in IPv6 subnet"};

    in6_addr networkAddress{};
    // Need a null-terminated string for inet_pton()
    auto addressString = brokenSubnet.first.to_string();
    if(inet_pton(AF_INET6, addressString.c_str(), &networkAddress) != 1)
        throw std::runtime_error{"invalid IPv4 address in subnet"};

    _prefix = static_cast<unsigned>(brokenSubnet.second);
    _address = Ipv6Address::maskIpv6(networkAddress.s6_addr, _prefix);
}

bool Ipv6Subnet::operator==(const Ipv6Subnet &other) const
{
    return _address == other._address && _prefix == other._prefix;
}

void Ipv6Subnet::trace(std::ostream &os) const
{
    os << _address << '/' << _prefix;
}

KACArraySlice toApi(ArraySlice<const Ipv4Address> addrs)
{
    // No addresses; &addrs.first()->address_ref() might be fine since it does
    // not actually dereference anything, but it's not guaranteed to work so
    // don't rely on it.
    if(addrs.empty())
        return {nullptr, 0, sizeof(Ipv4Address)};
    // Take the address of the
    return {&addrs.front().addressRef(), addrs.size(), sizeof(Ipv4Address)};
}

}}
