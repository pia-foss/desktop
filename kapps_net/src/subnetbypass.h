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
#include "firewallparams.h"
#include "originalnetworkscan.h"
#include "routemanager.h"
#include <string>
#include <set>
#include <memory>
#include <vector>

namespace kapps { namespace net {

class KAPPS_NET_EXPORT SubnetBypass
{
public:
    // Inject the RouteManager dependency - also makes
    // this class easily testable
    SubnetBypass(std::unique_ptr<RouteManager> routeManager)
        : _routeManager{std::move(routeManager)}
        , _isEnabled{false}
    {}

    void updateRoutes(const FirewallParams &params);
private:
    void addAndRemoveSubnets4(const FirewallParams &params);
    void addAndRemoveSubnets6(const FirewallParams &params);
    void clearAllRoutes4();
    void clearAllRoutes6();
    std::string boolToString(bool value) {return value ? "ON" : "OFF";}
    std::string stateChangeString(bool oldValue, bool newValue);
private:
    std::unique_ptr<RouteManager> _routeManager;
    OriginalNetworkScan _netScan;
    std::set<std::string> _ipv4Subnets;
    std::set<std::string> _ipv6Subnets;
    bool _isEnabled;
};

}}
