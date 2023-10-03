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
#include "dedicatedip.h"
#include "manualregion.h"
#include "region.h"
#include <kapps_core/stringslice.h>

#ifdef __cplusplus
extern "C" {
#endif

// The Region List describes all regions, servers, and services available to the
// client application.
//
// (For brands with "server groups", it represents a single server group; as
// each server group provides a separate region list.)
//
// The data are heirarchical:
// 1. The region list contains a number of regions
// 2. Each region contains a number of servers
// 3. Each server offers a number of services (possibly including service-
//    specific data, such as the possible remote ports for a service)
//
// Note that although the JSON model abbreviates service configurations using
// "service groups", this is intentionally not represented in the API.  Clients
// must not care about the service group structure; Ops is free to change this
// structure at any time.  The purpose of service groups are to reduce the size
// of the JSON for otherwise highly-repetitive service configurations (since,
// realistically, only a few server configurations occur in the fleet at any
// given time).
//
// From clients' perspective, there are simply regions containing servers with
// any arbitrary combination of services each.  Clients cannot assume (say) that
// all OpenVPN UDP servers also offer OpenVPN TCP, which may not be true in the
// future.
typedef struct KARRegionList KARRegionList;

// ================
// Object Ownership
// ================
//
// Throughout the kapps modules, object ownership follows the following
// conventions (which will be familiar if you have worked with CoreFoundation on
// Apple platforms):
//
// * "Create" or "Copy" methods transfer ownership to the caller.  The caller
//   is responsible for destroying or releasing the object.  (For retainable
//   objects, this means you are given a reference, don't retain again unless
//   you mean to have two references.)
// * "Get" methods, or any other property accessors, like KARRegionListRegions(),
//   do not transfer ownership.  Ownership remains with the called object
//   (KARRegionList in this example).  The object typically remains valid at
//   least until any mutator is called on its owner, or the owner is destroyed.
//   If you want to keep it longer, retain a reference (and later release it).
//
// For example:
// ```
// const KARRegionList *pRegionList = KARRegionListCreate(...);
// const KARRegion *pUsChicago = KARRegionListGetRegion(pRegionList, buildKacStringSlice("us_chicago"));
// /* make sure to verify that pUsChicago was found */
// printf("US Chicago has %zd servers", KARRegionServers(pUsChicago).size);
// KARRegionListDestroy(pRegionList);
// /* pUsChicago is no longer valid, caller did not retain a reference before
//    destroying region list */
// ```
//
// Or, instead:
// ```
// const KARRegionList *pRegionList = KARRegionListCreate(...);
// const KARRegion *pSpain = KARRegionListGetRegion(pRegionList, buildKacStringSlice("spain"));
// /* make sure to verify that pSpain was found */
// /* retain pSpain to use it later */
// KARRegionRetain(pSpain);
// KARRegionListDestroy(pRegionList);
// /* we can still use pSpain since we own a reference */
// printf("Spain has %zd servers", KARRegionServers(pSpain).size);
// /* release it when you're done */
// KARRegionRelease(pSpain);
// /* pSpain is no longer valid, caller's reference was released */
// ```

// Build a region list from regions JSON.  Remove any attached signature if
// present; the input is plain JSON.
//
// Semantic errors in individual servers or regions are traced and ignored (the
// region or server is ignored, i.e. in the event of missing or invalid fields).
// If the entire JSON is unusable (syntactically invalid, defines no regions,
// etc.), then nullptr is returned.
//
// In addition to the regions list JSON, provide:
// - Shadowsocks region list JSON, if the client intends to use Shadowsocks. If
//   given, the Shadowsocks servers are incorporated into the region list.
// - Descriptions of the user's Dedicated IPs, which are incorporated as
//   "DIP regions" (see KARDedicatedIP).  Often there are either 0 or 1 DIP, but
//   in principle 0 or more can be provided.
// - Description of any manual regions, if desired (see KARManualRegion)
KAPPS_REGIONS_EXPORT
const KARRegionList *KARRegionListCreate(KACStringSlice regionsJson,
                                         KACStringSlice shadowsocksJson,
                                         const KARDedicatedIP *pDIPs, size_t dipCount,
                                         const KARManualRegion *pManualRegions, size_t manualCount);
// Build a region list from legacy PIAv6 regions JSON.  As above, remove any
// attached signature if present.  Dedicated IPs and manual servers can be
// specified, but note that their listed server groups must correspond to the
// PIAv6 JSON.
//
// This provides complete data since all the needed data are also present in
// PIAv6.  (Note that the corresponding Metadata legacy support has caveats.)
KAPPS_REGIONS_EXPORT
const KARRegionList *KARRegionListCreatePiav6(KACStringSlice regionsv6Json,
                                              KACStringSlice shadowsocksJson,
                                              const KARDedicatedIP *pDIPs, size_t dipCount,
                                              const KARManualRegion *pManualRegions, size_t manualCount);
// Destroy a region list.
KAPPS_REGIONS_EXPORT
void KARRegionListDestroy(const KARRegionList *pRegionList);

// Get the public DNS servers, if provided. These use anycast routing, so they
// can be used from any geographic location - they're intended to be used when
// not connected to the VPN if DNS is needed.
//
// Some brands may not provide this, in which case the list is empty.
//
// The array elements are KACIPv4Address values (uint32_t)
KAPPS_REGIONS_EXPORT
KACArraySlice KARRegionListPublicDnsServers(const KARRegionList *pRegionList);

// Get a region by ID.  IDs are case-sensitive.  Returns nullptr if the ID
// is not known.  The region remains owned by this region list.
KAPPS_REGIONS_EXPORT
const KARRegion *KARRegionListGetRegion(const KARRegionList *pRegionList,
                                        KACStringSlice id);

// Get all regions, as an array slice with KARRegion* elements (note that array
// elements are pointers).  Can be used to enumerate all regions.
KAPPS_REGIONS_EXPORT
KACArraySlice KARRegionListRegions(const KARRegionList *pRegionList);

#ifdef __cplusplus
}
#endif
