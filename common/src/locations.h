// Copyright (c) 2021 Private Internet Access, Inc.
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

#include "common.h"
#line HEADER_FILE("locations.h")

#ifndef LOCATIONS_H
#define LOCATIONS_H

#include "settings.h"



// Build Location and Server objects for the modern region infrastructure from
// the latencies and modern regions list.
// The legacy Shadowsocks list is also taken so we can leverage legacy
// Shadowsocks servers; the modern infrastructure does not have Shadowsocks yet.
// Dedicated IPs are added as additional regions.
COMMON_EXPORT LocationsById buildModernLocations(const LatencyMap &latencies,
                                                 const QJsonObject &regionsObj,
                                                 const QJsonObject &legacyShadowsocksObj,
                                                 const std::vector<AccountDedicatedIp> &dedicatedIps);

// Build the grouped and sorted locations from the flat locations.
COMMON_EXPORT void buildGroupedLocations(const LocationsById &locations,
                                         std::vector<CountryLocations> &groupedLocations,
                                         std::vector<QSharedPointer<Location>> &dedicatedIpLocations);

class COMMON_EXPORT NearestLocations
{
public:
    NearestLocations(const LocationsById &locations);

public:
    // Find the closest server location that is safe to use with 'connect auto'.
    // This only considers non-geo servers that are indicated for auto selection
    // in the servers list.
    //
    // If the portForward parameter is true, then prefer the closest location
    // that supports port forwarding.  (Still fall back to the closest location
    // if none support PF.)
    //
    // This uses the selection algorithm below; portForward is applied as a
    // context-specific requirement if set.
    QSharedPointer<Location> getNearestSafeVpnLocation(bool portForward) const;

    // "Auto" location selections consider three criteria to try to pick the
    // nearest location that meets requirements:
    //
    // 1. A context-specific requirement - supports Shadowsocks or supports port
    //    forwarding.
    // 2. Auto safe - indicated by regions list
    // 3. Not geo - geo locations are almost always colocated with physical
    //    locations, so there's no latency benefit to selecting a geo location.
    //    The auto region selection would flutter since the latencies would be
    //    very close.
    //
    // If no locations match all criteria, the selection will fall back to try
    // to match the most important criteria. The precedence order for an auto
    // location selection is:
    //
    // | # | Context | Auto safe | Not geo |
    // |---|---------|-----------|---------|
    // | 1 | X       | X         | X       |
    // | 2 | X       | X         | -       | (No non-geo regions match context)
    // | 3 | X       | -         | X       | (No non-geo auto-safe regions match context)
    // | 4 | X       | -         | -       | (No auto-safe regions match context)
    // | 5 | -       | X         | X       | (No regions match context)
    // | 6 | -       | X         | -       | (No regions match context, and no non-geo regions are auto-safe)
    // | 7 | -       | -         | X       | (No regions match context, and no regions are auto-safe)
    // | 8 | -       | -         | -       | (No regions match context, and there are no auto-safe regions)
    //
    // The context-specific requirement might not be possible to drop -
    // "requires Shadowsocks" is a hard requirement, but "supports port
    // forwarding" can be dropped.  This is handled contextually by falling back
    // from getBestMatchingLocation() to getBestLocation() if possible.

    // Find the closest server location that is safe, non-geo, and satisfies an
    // arbitrary predicate.
    //
    // Will fall back to geo and/or non-auto-safe locations if necessary to
    // match the predicate, per above.  Does _not_ fall back to locations that
    // do not match the predicate; use getBestLocation() as a fallback if that
    // is possible and this method fails to find a location.
    template<class LocationTestFunc>
    QSharedPointer<Location> getBestMatchingLocation(LocationTestFunc isAllowedBase) const
    {
        if(_locations.empty())
        {
            qWarning() << "There are no available Server Locations!";
            return {};
        }

        // Ensure we never select an offline region
        auto isAllowed = [&isAllowedBase](const Location &loc) {
            return !loc.offline() && isAllowedBase(loc);
        };

        // Find the nearest matching server that's safe for auto and not geo.
        auto itResult = std::find_if(_locations.begin(), _locations.end(),
            [&isAllowed](const auto &pLocation)
            {
                return pLocation && pLocation->autoSafe() &&
                    !pLocation->geoOnly() && isAllowed(*pLocation);
            });
        if(itResult != _locations.end())
            return *itResult;

        // No matching locations are safe for auto and not geo.  Prefer auto
        // safe; allow geo.
        itResult = std::find_if(_locations.begin(), _locations.end(),
            [&isAllowed](const auto &pLocation)
            {
                return pLocation && pLocation->autoSafe() && isAllowed(*pLocation);
            });
        if(itResult != _locations.end())
            return *itResult;

        // No auto-safe location is allowed.  Prefer a non-geo location.
        itResult = std::find_if(_locations.begin(), _locations.end(),
            [&isAllowed](const auto &pLocation)
            {
                return pLocation && !pLocation->geoOnly() && isAllowed(*pLocation);
            });
        if(itResult != _locations.end())
            return *itResult;

        // Allow any location regardless of auto/geo.
        itResult = std::find_if(_locations.begin(), _locations.end(),
            [&isAllowed](const auto &pLocation)
            {
                return pLocation && isAllowed(*pLocation);
            });
        if(itResult != _locations.end())
            return *itResult;

        // Nothing is allowed by context.  Caller can fall back to
        // getBestLocation() if possible.
        return {};
    }

    QSharedPointer<Location> getBestLocation() const
    {
        // No context criterion, just use a default predicate.
        return getBestMatchingLocation([](const Location &){return true;});
    }

private:
    std::vector<QSharedPointer<Location>> _locations;
};

#endif
