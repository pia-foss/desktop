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
#include "regions.h"
#include "service.h"
#include <kapps_core/stringslice.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// A Server is exactly one remote IP address from which one or more services is
// offered.  Each Server indicates the services offered and various
// configuration parameters about those services (such as available remote
// ports, etc.)
//
// Server objects are created by a region list.
typedef struct KARServer KARServer;
KAPPS_REGIONS_EXPORT
void KARServerRetain(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
void KARServerRelease(const KARServer *pServer);

// IPv4 address - globally-routable address where the services can be reached.
// Note that for VPN services, this is _not_ necessarily the "exit" address that
// will be observed by remote connections through the VPN.
KAPPS_REGIONS_EXPORT
KACIPv4Address KARServerIpAddress(const KARServer *pServer);

// X.509 common name to expect in this server's certificate provided by
// our internal CA.
KAPPS_REGIONS_EXPORT
KACStringSlice KARServerCommonName(const KARServer *pServer);

// FQDN - optional, some brands and servers do not provide this.  Only needed
// for some platforms' IKEv2 implementations.
KAPPS_REGIONS_EXPORT
KACStringSlice KARServerFqdn(const KARServer *pServer);

// Services - these properties indicate the provided services and their
// configuration information on this server.  Note that the server group name is
// intentionally not provided; clients must not care about server grouping -
// instead look for the needed services (and ports, etc.)

// General APIs - these just call one of the specific methods based on the
// service specified.
//
// HasService() works for all services.  Ports() only works for services that
// actually provide ports - note that IKEv2 does not, and note that the precise
// meaning of the ports varies by service.
//
// Note also that a few services provide additional configuration information
// (OpenVPN NCP, Shadowsocks ciphers, etc.)
KAPPS_REGIONS_EXPORT
bool KARServerHasService(const KARServer *pServer, KARService service);
KAPPS_REGIONS_EXPORT
KACPortArray KARServerServicePorts(const KARServer *pServer, KARService service);

// OpenVPN UDP - If supported, an array of available remote ports is provided.
// (Testing for the OpenVPN UDP service is equivalent to testing if at least one
// port is provided here.)
//
// The service also indicates whether to use NCP for cipher negotiation - if
// not, pia-signal-settings is used.  This still exists in PIA but is being
// deprecated in favor of NCP.
KAPPS_REGIONS_EXPORT
bool KARServerHasOpenVpnUdp(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
KACPortArray KARServerOpenVpnUdpPorts(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
bool KARServerOpenVpnUdpNcp(const KARServer *pServer);

// OpenVPN TCP - just as OpenVPN UDP, indicates the available ports.  Note that
// NCP is provided separately for TCP because it is defined as a service
// parameter - it's unlikely that a server would differ between UDP and TCP, but
// the regions list allows it.
KAPPS_REGIONS_EXPORT
bool KARServerHasOpenVpnTcp(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
KACPortArray KARServerOpenVpnTcpPorts(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
bool KARServerOpenVpnTcpNcp(const KARServer *pServer);

// WireGuard - also provides an array of available ports.  Unlike OpenVPN, note
// that a remote port refers to _both_ a TCP and UDP port (TCP for provisioning;
// UDP for data).
//
// Currently, only one port is deployed, so we assume that the same TCP and UDP
// ports must be used for both provisioning and data (i.e. we cannot provision
// using TCP 1000 and then pass data using UDP 2000).
KAPPS_REGIONS_EXPORT
bool KARServerHasWireGuard(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
KACPortArray KARServerWireGuardPorts(const KARServer *pServer);

// IKEv2.  No additional information is specified, as IKEv2 implementations
// leverage built-in OS support, and no OSes allow us to specify custom ports.
// (OS support is the only real reason to offer IKEv2; i.e. it does not require
// a driver on Windows, it is the only in-kernel option on some platforms, etc.)
KAPPS_REGIONS_EXPORT
bool KARServerHasIkev2(const KARServer *pServer);

// Shadowsocks proxies.  Ports are provided; our proxies currently provide TCP
// only.  Key and cipher are also provided; note that both are public since
// Shadowsocks lacks any individualized authentication mechanism.
KAPPS_REGIONS_EXPORT
bool KARServerHasShadowsocks(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
KACPortArray KARServerShadowsocksPorts(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
KACStringSlice KARServerShadowsocksKey(const KARServer *pServer);
// The Shadowsocks cipher name is provided using a name understood by
// shadowsocks-libev.
KAPPS_REGIONS_EXPORT
KACStringSlice KARServerShadowsocksCipher(const KARServer *pServer);

// "Meta" API proxies - can be used to reach the web API through the VPN servers
// themselves.  These are HTTPS, so the available ports refer to TCP ports.
KAPPS_REGIONS_EXPORT
bool KARServerHasMeta(const KARServer *pServer);
KAPPS_REGIONS_EXPORT
KACPortArray KARServerMetaPorts(const KARServer *pServer);

#ifdef __cplusplus
}
#endif
