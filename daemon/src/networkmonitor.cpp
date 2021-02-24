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
#line SOURCE_FILE("networkmonitor.cpp")

#include "networkmonitor.h"

NetworkConnection::NetworkConnection(const QString &networkInterface,
                                     bool defaultIpv4, bool defaultIpv6,
                                     const Ipv4Address &gatewayIpv4,
                                     const Ipv6Address &gatewayIpv6,
                                     std::vector<std::pair<Ipv4Address, unsigned>> addressesIpv4,
                                     std::vector<std::pair<Ipv6Address, unsigned>> addressesIpv6)
    : _networkInterface{networkInterface},
      _defaultIpv4{defaultIpv4}, _defaultIpv6{defaultIpv6},
      _gatewayIpv4{gatewayIpv4}, _gatewayIpv6{gatewayIpv6},
      _addressesIpv4{std::move(addressesIpv4)},
      _addressesIpv6{std::move(addressesIpv6)}
{
    // Sort the local IP addresses so we can check for equality by just
    // comparing the vectors
    std::sort(_addressesIpv4.begin(), _addressesIpv4.end());
    // Ignore link-local addresses since we don't get them for all platforms
    _addressesIpv6.erase(std::remove_if(_addressesIpv6.begin(), _addressesIpv6.end(),
                                        [](const auto &addr){return addr.first.isLinkLocal();}),
                         _addressesIpv6.end());
    std::sort(_addressesIpv6.begin(), _addressesIpv6.end());
}

bool NetworkConnection::operator<(const NetworkConnection &other) const
{
    auto cmp = networkInterface().compare(other.networkInterface());
    if(cmp != 0)
        return cmp < 0;
    if(gatewayIpv4() != other.gatewayIpv4())
        return gatewayIpv4() < other.gatewayIpv4();
    if(gatewayIpv6() != other.gatewayIpv6())
        return gatewayIpv6() < other.gatewayIpv6();
    if(addressesIpv4() != other.addressesIpv4())
    {
        return std::lexicographical_compare(addressesIpv4().begin(), addressesIpv4().end(),
                                            other.addressesIpv4().begin(), other.addressesIpv4().end());
    }
    if(addressesIpv6() != other.addressesIpv6())
    {
        return std::lexicographical_compare(addressesIpv6().begin(), addressesIpv6().end(),
                                            other.addressesIpv6().begin(), other.addressesIpv6().end());
    }
    return false;
}

void NetworkMonitor::updateNetworks(std::vector<NetworkConnection> newNetworks)
{
    if(newNetworks != _lastNetworks)
    {
        _lastNetworks = std::move(newNetworks);
        emit networksChanged(_lastNetworks);
    }
}
