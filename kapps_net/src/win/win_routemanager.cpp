// Copyright (c) 2022 Private Internet Access, Inc.
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

#include "win_routemanager.h"
#include <kapps_core/src/win/win_error.h>

namespace kapps { namespace net {

void WinRouteManager::createRouteEntry(MIB_IPFORWARD_ROW2 &route, const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric) const
{
    InitializeIpForwardEntry(&route);
    NET_LUID luid{};

    try
    {
        luid.Value = static_cast<ULONG64>(std::stoull(interfaceName));
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Unable to parse interface" << interfaceName
            << "-" << ex.what();
        // Ignored for consistency with prior QString implementation, should not
        // happen
    }
    route.InterfaceLuid = luid;

    // For consistency with prior QHostAddress implementation, treat unparseable
    // subnets as 0/32; should not happen here (subnets have already been
    // validated)
    core::Ipv4Subnet subnetValue{{}, 32};
    try
    {
        subnetValue = core::Ipv4Subnet{subnet};
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Unable to parse subnet" << subnet << "-"
            << ex.what();
    }

    // Destination subnet
    route.DestinationPrefix.Prefix.si_family = AF_INET;
    route.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr = htonl(subnetValue.address().address());
    route.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
    route.DestinationPrefix.PrefixLength = subnetValue.prefix();

    // Router address (next hop)
    route.NextHop.Ipv4.sin_addr.s_addr = htonl(core::Ipv4Address{gatewayIp}.address());
    route.NextHop.Ipv4.sin_family = AF_INET;

    route.Metric = metric;
}

void WinRouteManager::addRoute4(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric) const
{
    MIB_IPFORWARD_ROW2 route{};
    createRouteEntry(route, subnet, gatewayIp, interfaceName, metric);

    KAPPS_CORE_INFO() << "Adding route for" << subnet << "via" << interfaceName
        << "with metric:" << metric;
    // Add the routing entry
    auto routeResult = CreateIpForwardEntry2(&route);
    if(routeResult != NO_ERROR)
    {
        KAPPS_CORE_WARNING() << "Could not create route for" << subnet << "-"
            << core::WinErrTracer{routeResult};
    }
}

void WinRouteManager::removeRoute4(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName) const
{
    MIB_IPFORWARD_ROW2 route{};
    createRouteEntry(route, subnet, gatewayIp, interfaceName, 0);
    KAPPS_CORE_INFO() << "Removing route for" << subnet << "via" << interfaceName;

    // Delete the routing entry
    if(DeleteIpForwardEntry2(&route) != NO_ERROR)
    {
        KAPPS_CORE_WARNING() << "Could not delete route for" << subnet;
    }
}

}}
