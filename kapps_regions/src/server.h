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
#include <kapps_core/src/ipaddress.h>
#include <kapps_core/src/stringslice.h>
#include "servicegroup.h"
#include <kapps_core/src/retainshared.h>
#include <kapps_regions/regions.h>
#include <cassert>
#include <string>
#include <memory>

namespace kapps::regions {

class KAPPS_REGIONS_EXPORT Server : public core::RetainSharedFromThis<Server>
{
public:
    Server(core::Ipv4Address address, std::string commonName, std::string fqdn,
           std::shared_ptr<ServiceGroup> pServiceGroup)
        : _address{address}, _commonName{std::move(commonName)},
          _fqdn{std::move(fqdn)}, _pServiceGroup{std::move(pServiceGroup)}
    {
        assert(_pServiceGroup); // Ensured by caller
    }

public:
    core::Ipv4Address address() const {return _address;}
    core::StringSlice commonName() const {return _commonName;}
    core::StringSlice fqdn() const {return _fqdn;}

    bool hasService(Service service) const;
    Ports servicePorts(Service service) const;

    bool hasOpenVpnUdp() const {return !openVpnUdpPorts().empty();}
    Ports openVpnUdpPorts() const {return _pServiceGroup->openVpnUdpPorts();}
    bool openVpnUdpNcp() const {return _pServiceGroup->openVpnUdpNcp();}

    bool hasOpenVpnTcp() const {return !openVpnTcpPorts().empty();}
    Ports openVpnTcpPorts() const {return _pServiceGroup->openVpnTcpPorts();}
    bool openVpnTcpNcp() const {return _pServiceGroup->openVpnTcpNcp();}

    bool hasWireGuard() const {return !wireGuardPorts().empty();}
    Ports wireGuardPorts() const {return _pServiceGroup->wireGuardPorts();}

    bool hasIkev2() const {return _pServiceGroup->ikev2();}

    bool hasShadowsocks() const {return !shadowsocksPorts().empty();}
    Ports shadowsocksPorts() const {return _pServiceGroup->shadowsocksPorts();}
    core::StringSlice shadowsocksKey() const {return _pServiceGroup->shadowsocksKey();}
    core::StringSlice shadowsocksCipher() const {return _pServiceGroup->shadowsocksCipher();}

    bool hasMeta() const {return !metaPorts().empty();}
    Ports metaPorts() const {return _pServiceGroup->metaPorts();}

private:
    core::Ipv4Address _address;
    std::string _commonName;
    std::string _fqdn;
    std::shared_ptr<ServiceGroup> _pServiceGroup;
};

}
