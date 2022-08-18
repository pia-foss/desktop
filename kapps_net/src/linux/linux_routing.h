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
#include <kapps_core/core.h>
#include <kapps_net/net.h>
#include <string>
#include <assert.h>

namespace kapps { namespace net {

class KAPPS_NET_EXPORT Routing
{
public:
    // Priorities for our routing policies. Lower values mean higher priority.
    // - suppressedMain -> from all lookup main suppress_prefixlength 1
    // - vpnOnly        -> from all fwmark <Fwmark::vpnOnlyPacketTag> lookup piavpnOnlyrt
    // - bypass         -> from all fwmark <Fwmark::excludePacketTag> lookup piavpnrt
    // - sourceIp       -> from <physical ip> lookup piavpnrt
    // - wireguard      -> from all not fwmark <Fwmark::wireguardFwmark> lookup piavpnWgrt

    enum Priorities{
        suppressedMain = 50,
        forwarded = 70,
        vpnOnly = 100,
        bypass = 101,
        sourceIp = 102,
        wireguard = 102
    };

public:
    Routing(std::string brandCode) : _brandCode{std::move(brandCode)} {}

public:
    std::string bypassTable() const {return _brandCode + "vpnrt";}
    std::string vpnOnlyTable() const {return _brandCode + "vpnOnlyrt";}
    std::string wireguardTable() const {return _brandCode + "vpnWgrt";}
    std::string forwardedTable() const {return _brandCode + "vpnFwdrt";}

private:
    std::string _brandCode;
};

}}
