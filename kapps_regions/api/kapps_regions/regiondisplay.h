// Copyright (c) 2024 Private Internet Access, Inc.
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
#include <kapps_core/stringslice.h>

#ifdef __cplusplus
extern "C" {
#endif

// Region display information - dots, map coordinates, etc.
typedef struct KARRegionDisplay KARRegionDisplay;
KAPPS_REGIONS_EXPORT
void KARRegionDisplayRetain(const KARRegionDisplay *pRegion);
KAPPS_REGIONS_EXPORT
void KARRegionDisplayRelease(const KARRegionDisplay *pRegion);

// Get the region's ID.
KAPPS_REGIONS_EXPORT
KACStringSlice KARRegionDisplayId(const KARRegionDisplay *pRegion);
// Get the region's country code (ISO 3166-1 alpha-2)
KAPPS_REGIONS_EXPORT
KACStringSlice KARRegionDisplayCountry(const KARRegionDisplay *pRegion);
// Get the region's geographic latitude - [-90, 90] corresponds to [90 deg S,
// 90 deg N].  Returns NaN if the coordinates are not available.
KAPPS_REGIONS_EXPORT
double KARRegionDisplayGeoLatitude(const KARRegionDisplay *pRegion);
// Get the region's geographic longitude - [-180, 180] corresponds to
// [180 deg W, 180 deg E].  Returns NaN if the coordinates are not available.
KAPPS_REGIONS_EXPORT
double KARRegionDisplayGeoLongitude(const KARRegionDisplay *pRegion);
// Get the region's display name translations.
KAPPS_REGIONS_EXPORT
const KARDisplayText *KARRegionDisplayName(const KARRegionDisplay *pRegion);

#ifdef __cplusplus
}
#endif
