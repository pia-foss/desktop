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
#include "server.h"
#include <kapps_core/stringslice.h>
#include <kapps_core/arrayslice.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// A Region contains one or more servers, as well as regional information - its
// ID, and some flags such as whether it's safe to use automatically, has port
// forwarding, is geo-located, etc.
//
// Region objects are created by creating a region list, but they can be
// retained by callers.
typedef struct KARRegion KARRegion;
KAPPS_REGIONS_EXPORT
void KARRegionRetain(const KARRegion *pRegion);
KAPPS_REGIONS_EXPORT
void KARRegionRelease(const KARRegion *pRegion);

// Get the region's ID.  IDs are case-sensitive.
KAPPS_REGIONS_EXPORT
KACStringSlice KARRegionId(const KARRegion *pRegion);

// Whether the region is safe to use an automatic selection
KAPPS_REGIONS_EXPORT
bool KARRegionAutoSafe(const KARRegion *pRegion);
// Whether the region supports port forwarding
KAPPS_REGIONS_EXPORT
bool KARRegionPortForward(const KARRegion *pRegion);
// Whether the region is geo-located
KAPPS_REGIONS_EXPORT
bool KARRegionGeoLocated(const KARRegion *pRegion);

// Whether the region is offline - a region is offline if and only if it has no
// servers (equivalent to checking server count == 0)
KAPPS_REGIONS_EXPORT
bool KARRegionOffline(const KARRegion *pRegion);

// Whether the region is a DIP region (just tests that the DIP address != 0)
KAPPS_REGIONS_EXPORT
bool KARRegionIsDedicatedIp(const KARRegion *pRegion);
// If the region is a DIP region, the DIP address.  This is also the address of
// each server providing VPN services, but note that other servers will also
// appear in this region for 'meta', etc.
//
// Returns zero if the region is not a DIP region.
KAPPS_REGIONS_EXPORT
KACIPv4Address KARRegionDedicatedIpAddress(const KARRegion *pRegion);

// Test whether any server in the region offers the specified service.  For
// services offering multiple ports, this indicates that at least one port is
// available on at least one server.
//
// This is equivalent to examining all servers in the region.
KAPPS_REGIONS_EXPORT
bool KARRegionHasService(const KARRegion *pRegion, KARService service);

// Get the first server for a specified service.  This is typically useful for
// auxiliary services like Meta or Shadowsocks; actual VPN connections should
// use a more sophisticated strategy.
//
// Returns nullptr only if no servers offer this service; equivalent to finding
// the first server in the list with this service.
KAPPS_REGIONS_EXPORT
const KARServer *KARRegionFirstServerFor(const KARRegion *pRegion, KARService service);

// Get all the servers in this region as an array slice with KARServer* elements
// (note pointer elements).  Note that each server specifies its offered
// services, so to get all servers for a particular service (or for a particular
// service and port, etc.), filter the results.
KAPPS_REGIONS_EXPORT
KACArraySlice KARRegionServers(const KARRegion *pRegion);

#ifdef __cplusplus
}
#endif
