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
#include <kapps_core/src/stringslice.h>
#include <kapps_regions/service.h>
#include <kapps_core/src/corejson.h>
#include <cstdint>
#include <vector>
#include <string>

namespace kapps::regions {

// Prefer the properly-scoped C++ enumeration in the C++ internals, but make
// sure each element is numerically equal to the external C enum so we can
// simply cast for the API.
enum class Service
{
    OpenVpnTcp = KARServiceOpenVpnTcp,
    OpenVpnUdp = KARServiceOpenVpnUdp,
    WireGuard = KARServiceWireGuard,
    Ikev2 = KARServiceIkev2,
    Shadowsocks = KARServiceShadowsocks,
    Meta = KARServiceMeta
};

// This is a mock implementation of the API while the existing internals are
// ported and adapted from PIA Desktop.

// Array slice of immutable ports - this is used a lot.
using Ports = core::ArraySlice<const std::uint16_t>;

// Though Server doesn't expose its service group directly, we still store the
// service group information in a "service group" so we don't have to copy this
// tons of times.
class KAPPS_REGIONS_EXPORT ServiceGroup : public core::JsonReadable<ServiceGroup>
{
public:
    ServiceGroup() = default;
    ServiceGroup(std::vector<std::uint16_t> openVpnUdpPorts, bool openVpnUdpNcp,
                 std::vector<std::uint16_t> openVpnTcpPorts, bool openVpnTcpNcp,
                 std::vector<std::uint16_t> wireGuardPorts,
                 bool ikev2,
                 std::vector<std::uint16_t> shadowsocksPorts,
                 std::string shadowsocksKey, std::string shadowsocksCipher,
                 std::vector<std::uint16_t> metaPorts);

private:
    // Read any JSON service definition that specifies a list of ports
    // (includes OpenVPN UDP/TCP, WireGuard, meta)
    void readJsonServicePorts(const nlohmann::json &service,
                              const core::StringSlice &serviceId,
                              std::vector<std::uint16_t> &ports);
    // Read any OpenVPN service (UDP or TCP); these have ports as well as an
    // 'ncp' flag.
    void readJsonServiceOpenVpn(const nlohmann::json &service,
                                const core::StringSlice &serviceId,
                                std::vector<std::uint16_t> &ports, bool &ncp);
    // Read a service group's services from the JSON "services" array.  Used to
    // implement readJson() and readPiav6JsonServicesArray()
    void readJsonServicesArray(const nlohmann::json &services,
                               const std::string &serviceNameKey);

public:
    // Test whether this service group has any known service.  Used to ignore
    // empty service groups in RegionList.
    bool hasAnyService() const;

    Ports openVpnUdpPorts() const {return _openVpnUdpPorts;}
    bool openVpnUdpNcp() const {return _openVpnUdpNcp;}

    Ports openVpnTcpPorts() const {return _openVpnTcpPorts;}
    bool openVpnTcpNcp() const {return _openVpnTcpNcp;}

    Ports wireGuardPorts() const {return _wireGuardPorts;}

    bool ikev2() const {return _ikev2;}

    Ports shadowsocksPorts() const {return _shadowsocksPorts;}
    core::StringSlice shadowsocksKey() const {return _shadowsocksKey;}
    core::StringSlice shadowsocksCipher() const {return _shadowsocksCipher;}

    Ports metaPorts() const {return _metaPorts;}

    // ServiceGroup can be read from a JSON service group, but it does not use
    // the JSON 'name' field; RegionsList uses this internally to identify
    // service groups.
    void readJson(const nlohmann::json &j);
    // Read a legacy v6 format service group, which is just an array of
    // services.
    void readPiav6JsonServicesArray(const nlohmann::json &services);

private:
    std::vector<std::uint16_t> _openVpnUdpPorts;
    bool _openVpnUdpNcp;
    std::vector<std::uint16_t> _openVpnTcpPorts;
    bool _openVpnTcpNcp;
    std::vector<std::uint16_t> _wireGuardPorts;
    bool _ikev2;
    std::vector<std::uint16_t> _shadowsocksPorts;
    std::string _shadowsocksKey;
    std::string _shadowsocksCipher;
    std::vector<std::uint16_t> _metaPorts;
};

}
