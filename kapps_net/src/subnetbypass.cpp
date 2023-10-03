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

#include "subnetbypass.h"
#include <kapps_core/src/logger.h>
#include <kapps_core/src/util.h>

namespace kapps { namespace net {

void SubnetBypass::clearAllRoutes4()
{
    for(const auto &subnet : _ipv4Subnets)
        _routeManager->removeRoute4(subnet, _netScan.gatewayIp(), _netScan.interfaceName());

    _ipv4Subnets.clear();
}

void SubnetBypass::clearAllRoutes6()
{
    for(const auto &subnet : _ipv6Subnets)
        _routeManager->removeRoute6(subnet, _netScan.gatewayIp6(), _netScan.interfaceName());

    _ipv6Subnets.clear();
}

void SubnetBypass::addAndRemoveSubnets4(const FirewallParams &params)
{
    auto subnetsToRemove{qs::setDifference(_ipv4Subnets, params.bypassIpv4Subnets)};
    auto subnetsToAdd{qs::setDifference(params.bypassIpv4Subnets,  _ipv4Subnets)};


    // Remove routes for old subnets
    for(const auto &subnet : subnetsToRemove)
        _routeManager->removeRoute4(subnet, params.netScan.gatewayIp(), params.netScan.interfaceName());

    // Add routes for new subnets
    for(auto subnet : subnetsToAdd)
        _routeManager->addRoute4(subnet, params.netScan.gatewayIp(), params.netScan.interfaceName());
}

void SubnetBypass::addAndRemoveSubnets6(const FirewallParams &params)
{
    auto subnetsToRemove{qs::setDifference(_ipv6Subnets, params.bypassIpv6Subnets)};
    auto subnetsToAdd{qs::setDifference(params.bypassIpv6Subnets,  _ipv6Subnets)};

    // Remove routes for old subnets
    for(const auto &subnet : subnetsToRemove)
        _routeManager->removeRoute6(subnet, params.netScan.gatewayIp6(), params.netScan.interfaceName());

    // Add routes for new subnets
    for(auto subnet : subnetsToAdd)
        _routeManager->addRoute6(subnet, params.netScan.gatewayIp6(), params.netScan.interfaceName());
}

std::string SubnetBypass::stateChangeString(bool oldValue, bool newValue)
{
    if(oldValue != newValue)
        return qs::format("% -> %", boolToString(oldValue), boolToString(newValue));
    else
        return boolToString(oldValue);
}

void SubnetBypass::updateRoutes(const FirewallParams &params)
{
    // We only need to create routes if:
    // - split tunnel is enabled
    // - we've connected since enabling the VPN
    // - bypassing isn't already the default behavior
    // - the netScan is valid
    bool shouldBeEnabled = params.enableSplitTunnel &&
// We want bypass routes to continue to exist on macos even when disconnected - this is so
// they override the split tunnel routes.
// Also bypassDefaultApps doesn't prevent the split tunnel routes existing on macos, so we ignore that too.
#ifndef KAPPS_CORE_OS_MACOS
                           params.hasConnected &&
                           !params.bypassDefaultApps &&
#endif
                           params.netScan.ipv4Valid();

    KAPPS_CORE_INFO() << "SubnetBypass:" << stateChangeString(_isEnabled, shouldBeEnabled);

    // If subnet bypass is not enabled but was enabled previously, disable the subnet bypass
    if(!shouldBeEnabled && _isEnabled)
    {
        KAPPS_CORE_INFO() << "Clearing all subnet bypass routes";
        clearAllRoutes4();
        clearAllRoutes6();
        _isEnabled = false;
        _netScan = {};
    }
    // Otherwise, enable it
    else if(shouldBeEnabled)
    {
        // Wipe out all routes if the network changes. They'll get recreated
        // later if necessary
        if(params.netScan != _netScan)
        {
            KAPPS_CORE_INFO() << "Network info changed from"
                << _netScan << "to" << params.netScan << "Clearing routes";
            clearAllRoutes4();
            clearAllRoutes6();
        }

        if(params.bypassIpv4Subnets != _ipv4Subnets)
            addAndRemoveSubnets4(params);

        if(params.bypassIpv6Subnets != _ipv6Subnets)
            addAndRemoveSubnets6(params);

        _isEnabled = true;
        _ipv4Subnets = params.bypassIpv4Subnets;
        _ipv6Subnets = params.bypassIpv6Subnets;
        _netScan = params.netScan;
    }
}

}}
