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

class KAPPS_NET_EXPORT RouteManager
{
public:
    virtual void addRoute4(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric=0) const = 0;
    virtual void removeRoute4(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName) const = 0;
    virtual void addRoute6(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric=0) const = 0;
    virtual void removeRoute6(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName) const = 0;
    virtual ~RouteManager() = default;
};
