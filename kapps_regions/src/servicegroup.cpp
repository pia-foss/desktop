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

#include "servicegroup.h"
#include <kapps_core/src/logger.h>
#include <nlohmann/json.hpp>
#include <cassert>

namespace kapps::regions {

ServiceGroup::ServiceGroup(std::vector<std::uint16_t> openVpnUdpPorts, bool openVpnUdpNcp,
                           std::vector<std::uint16_t> openVpnTcpPorts, bool openVpnTcpNcp,
                           std::vector<std::uint16_t> wireGuardPorts,
                           bool ikev2,
                           std::vector<std::uint16_t> shadowsocksPorts,
                           std::string shadowsocksKey, std::string shadowsocksCipher,
                           std::vector<std::uint16_t> metaPorts)
    : _openVpnUdpPorts{std::move(openVpnUdpPorts)}, _openVpnUdpNcp{openVpnUdpNcp},
      _openVpnTcpPorts{std::move(openVpnTcpPorts)}, _openVpnTcpNcp{openVpnTcpNcp},
      _wireGuardPorts{std::move(wireGuardPorts)},
      _ikev2{ikev2},
      _shadowsocksPorts{std::move(shadowsocksPorts)},
      _shadowsocksKey{std::move(shadowsocksKey)},
      _shadowsocksCipher{std::move(shadowsocksCipher)},
      _metaPorts{std::move(metaPorts)}
{
}

void ServiceGroup::readJsonServicePorts(const nlohmann::json &service,
                                        const core::StringSlice &serviceId,
                                        std::vector<std::uint16_t> &ports)
{
    // Get the ports as a vector of std::uint16_t.  While json::get() can do
    // this automatically, it would allow silent float-to-integer conversions
    // and silent truncation; avoid permitting these since they are not valid
    // ports.
    const auto &jsonPorts = service.at("ports");
    if(!jsonPorts.is_array())
    {
        // Don't silently accept a non-array as an empty array (produce more
        // accurate error)
        KAPPS_CORE_WARNING() << "Service group had non-array value for ports in service"
            << serviceId << "-" << jsonPorts;
        throw std::runtime_error{"Service ports must be an array"};
    }

    std::vector<std::uint16_t> newPorts;
    newPorts.reserve(jsonPorts.size());
    for(const auto &port : jsonPorts)
    {
        auto portValue = port.get<std::uint64_t>();
        if(!port.is_number_unsigned() ||
            portValue < 1 ||
            portValue > std::numeric_limits<std::uint16_t>::max())
        {
            KAPPS_CORE_WARNING() << "Service" << serviceId
                << "contained invalid port" << port;
            throw std::runtime_error{"Service ports must be in range 1-65535"};
        }
        newPorts.push_back(static_cast<std::uint16_t>(portValue));
    }

    // Any service can only be defined in a service group once, and these
    // services must specify at least one port.  If there are already ports for
    // this service, it was defined more than once.
    if(!ports.empty())
    {
        KAPPS_CORE_WARNING() << "Service group specified service" << serviceId
            << "more than once - previously with ports:" << ports
            << "- now with ports:" << newPorts;
        throw std::runtime_error{"Service group contained duplicate service"};
    }

    if(newPorts.empty())
    {
        KAPPS_CORE_WARNING() << "Service group specified service" << serviceId
            << "with no ports";
        throw std::runtime_error{"Service group service with empty ports array"};
    }

    ports = std::move(newPorts);
}


void ServiceGroup::readJsonServiceOpenVpn(const nlohmann::json &service,
                                          const core::StringSlice &serviceId,
                                          std::vector<std::uint16_t> &ports,
                                          bool &ncp)
{
    readJsonServicePorts(service, serviceId, ports);
    // The NCP field is optional and defaults to true.
    ncp = true;
    auto itJsonNcp = service.find("ncp");
    if(itJsonNcp != service.end())
        ncp = itJsonNcp->get<bool>();
}

bool ServiceGroup::hasAnyService() const
{
    return !openVpnUdpPorts().empty() || !openVpnTcpPorts().empty() ||
        !wireGuardPorts().empty() || ikev2() || !shadowsocksPorts().empty() ||
        !metaPorts().empty();
}

void ServiceGroup::readJsonServicesArray(const nlohmann::json &services,
                                         const std::string &serviceNameKey)
{
    // Reset everything; ensures that services not specified in the JSON are
    // cleared
    *this = {};

    // Interpret each service in the array
    for(const auto &service : core::jsonArray(services))
    {
        // Each service must be an object with at least a service name.  (The
        // property name differs between v6 and v7.)  Other keys may be required
        // for specific service types.
        auto serviceId = service.at(serviceNameKey).get<core::StringSlice>();

        // Check for each of our supported services
        if(serviceId == "openvpn_udp")
        {
            readJsonServiceOpenVpn(service, serviceId, _openVpnUdpPorts,
                                   _openVpnUdpNcp);
        }
        else if(serviceId == "openvpn_tcp")
        {
            readJsonServiceOpenVpn(service, serviceId, _openVpnTcpPorts,
                                   _openVpnTcpNcp);
        }
        else if(serviceId == "wireguard")
            readJsonServicePorts(service, serviceId, _wireGuardPorts);
        else if(serviceId == "ikev2")
        {
            // IKEv2 has no configuration parameters; the ports cannot be
            // customized since no OS allows specifying custom ports.
            if(_ikev2)
            {
                KAPPS_CORE_WARNING() << "Service group specified IKEv2 service more than once";
                throw std::runtime_error{"Service group specified IKEv2 service more than once"};
            }
            _ikev2 = true;
        }
        else if(serviceId == "meta")
            readJsonServicePorts(service, serviceId, _metaPorts);
        // Otherwise, it's an unknown service, silently ignore it (to allow for
        // addition of future services).
    }

    // A service group _can_ contain no known services, such as if it only
    // contained new services that are unknown to this build.  The resulting
    // ServiceGroup is valid (with no services); RegionList then ignores servers
    // using that group since they have no known services.
}

void ServiceGroup::readJson(const nlohmann::json &j)
{
    // Service groups must have the 'services' array.  In the v7 format, service
    // names are in a property called "service"
    readJsonServicesArray(j.at("services"), "service");
}

void ServiceGroup::readPiav6JsonServicesArray(const nlohmann::json &services)
{
    // In the legacy v6 format, a service group is just an array of services.
    // In this format, service names are in a property called "name".
    readJsonServicesArray(services, "name");
}

}
