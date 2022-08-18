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

std::string hexNumberStr(unsigned value);

// These are fwmark values used by PIA on Linux.  We use these for both iptables
// and Wireguard, so they're centralized here to ensure that we allocate values
// correctly.
namespace kapps { namespace net {

class KAPPS_NET_EXPORT Fwmark
{
public:
    Fwmark(unsigned fwmarkBase) : _fwmarkBase{fwmarkBase} {}

public:
    void configure(unsigned fwmarkBase);

public:
    std::string excludePacketTag() const;
    std::string vpnOnlyPacketTag() const;
    std::string forwardedPacketTag() const;
    std::uint32_t wireguardFwmark() const;

private:
    unsigned _fwmarkBase;
};
}}
