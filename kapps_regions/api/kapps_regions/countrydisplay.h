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
#include "displaytext.h"
#include <kapps_core/stringslice.h>

#ifdef __cplusplus
extern "C" {
#endif

// Country display information - display names.
typedef struct KARCountryDisplay KARCountryDisplay;
KAPPS_REGIONS_EXPORT
void KARCountryDisplayRetain(const KARCountryDisplay *pCountry);
KAPPS_REGIONS_EXPORT
void KARCountryDisplayRelease(const KARCountryDisplay *pCountry);

// Get the country's ISO 3166-1 alpha-2 code.
KAPPS_REGIONS_EXPORT
KACStringSlice KARCountryDisplayCode(const KARCountryDisplay *pCountry);
// Get the display text for the country's name.
KAPPS_REGIONS_EXPORT
const KARDisplayText *KARCountryDisplayName(const KARCountryDisplay *pCountry);
// Get the display text for the country's "prefix" - used in some UIs when
// multiple regions are provided for a country.  (This is the "DE - " in
// "DE - Frankfurt", "DE - Berlin", etc.  Although it is the country code in
// en-US, other languages use other abbreviations.)
//
// Some brands do not provide this when the clients do not use it; in that case
// this KARDisplayText contains no language texts.
KAPPS_REGIONS_EXPORT
const KARDisplayText *KARCountryDisplayPrefix(const KARCountryDisplay *pCountry);

#ifdef __cplusplus
}
#endif
