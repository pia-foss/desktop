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

#pragma once
#include "../routemanager.h"
#include <kapps_net/net.h>
#include <kapps_core/src/logger.h>
#include <kapps_core/src/ipaddress.h>
#include <kapps_core/src/winapi.h>

namespace kapps { namespace net {

class KAPPS_NET_EXPORT WinRouteManager : public RouteManager
{
public:
    virtual void addRoute4(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric=0) const override;
    virtual void removeRoute4(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName) const override;

    // TODO: Implement these when we support IPv6
    virtual void addRoute6(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric=0) const override {}
    virtual void removeRoute6(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName) const override {}
private:
    void createRouteEntry(MIB_IPFORWARD_ROW2 &route, const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric) const;
};

}}
