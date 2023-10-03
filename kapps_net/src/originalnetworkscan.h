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
#include <kapps_net/net.h>
#include <string>

class KAPPS_NET_EXPORT OriginalNetworkScan
{
public:
    OriginalNetworkScan() = default;
    OriginalNetworkScan(const std::string &gatewayIp, const std::string &interfaceName,
                        const std::string &ipAddress, unsigned prefixLength,
                        unsigned mtu,
                        const std::string &ipAddress6, const std::string &gatewayIp6,
                        unsigned mtu6)
        : _gatewayIp{gatewayIp}, _interfaceName{interfaceName},
          _ipAddress{ipAddress}, _prefixLength{prefixLength}, _mtu{mtu},
          _ipAddress6{ipAddress6}, _gatewayIp6{gatewayIp6}, _mtu6{mtu6}
    {
    }

    bool operator==(const OriginalNetworkScan& rhs) const
    {
        return gatewayIp() == rhs.gatewayIp() &&
            interfaceName() == rhs.interfaceName() &&
            ipAddress() == rhs.ipAddress() &&
            prefixLength() == rhs.prefixLength() &&
            ipAddress6() == rhs.ipAddress6() &&
            gatewayIp6() == rhs.gatewayIp6();
    }

    bool operator!=(const OriginalNetworkScan& rhs) const
    {
        return !(*this == rhs);
    }

    // Check whether the OriginalNetworkScan has valid (non-empty) values for
    // all fields.
    bool ipv4Valid() const {return !gatewayIp().empty() && !interfaceName().empty() && !ipAddress().empty();}

    // Whether the host has IPv6 available (as a global IP)
    bool hasIpv6() const {return !ipAddress6().empty();}

public:
    void gatewayIp(const std::string &value) {_gatewayIp = value;}
    void interfaceName(const std::string &value) {_interfaceName = value;}
    void ipAddress(const std::string &value) {_ipAddress = value;}
    void prefixLength(unsigned value) {_prefixLength = value;}
    void mtu(unsigned value) { _mtu = value; }
    void ipAddress6(const std::string &value) {_ipAddress6 = value;}
    void gatewayIp6(const std::string &value) {_gatewayIp6 = value;}
    void mtu6(unsigned value) { _mtu6 = value; }

    const std::string &gatewayIp() const {return _gatewayIp;}
    const std::string &interfaceName() const {return _interfaceName;}
    const std::string &ipAddress() const {return _ipAddress;}
    unsigned prefixLength() const {return _prefixLength;}
    unsigned mtu() const { return _mtu; }

    // An IP address from the default IPv6 interface.  This may not be the same
    // interface as the default IPv4 interface reported above.
    const std::string &ipAddress6() const {return _ipAddress6;}
    const std::string &gatewayIp6() const {return _gatewayIp6;}
    // MTU for the default IPv6 interface.  (MTUs are currently only provided
    // on Windows.)  May differe from the IPv4 MTU even if the interfaces are
    // the same, because Windows has separate IPv4 and IPv6 MTUs for interfaces.
    unsigned mtu6() const { return _mtu6; }

private:
    std::string _gatewayIp, _interfaceName, _ipAddress;
    unsigned _prefixLength;
    unsigned _mtu;
    std::string _ipAddress6;
    std::string _gatewayIp6;
    unsigned _mtu6;
};


// Custom logging for OriginalNetworkScan instances
KAPPS_NET_EXPORT std::ostream &operator<<(std::ostream &os, const OriginalNetworkScan& netScan);
