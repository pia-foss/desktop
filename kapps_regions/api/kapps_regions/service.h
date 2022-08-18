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
#include "regions.h"

#ifdef __cplusplus
extern "C" {
#endif

// These are the known services able to be represented by the regions list.
// Each server can provide some or all of these services.  (All reported servers
// provide at least one service; servers with no known services are ignored.)
//
// Many of these are VPN services, but some other types of services also exist.
// Some service types carry additional information (available ports, etc.), but
// this information can vary per service.
typedef enum KARService
{
    // VPN protocol services
    KARServiceOpenVpnTcp,   // OpenVPN with TCP transport
    KARServiceOpenVpnUdp,   // OpenVPN with UDP transport
    KARServiceWireGuard,    // WireGuard with our custom REST API provisioning
    KARServiceIkev2,    // IKEv2 VPN
    // Other services
    KARServiceShadowsocks,  // Shadowsocks obfuscated proxy; TCP
    KARServiceMeta, // "Meta" API proxies
} KARService;

#ifdef __cplusplus
}
#endif
