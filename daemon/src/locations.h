// Copyright (c) 2020 Private Internet Access, Inc.
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

// Build a single legacy server that provides one service with the given ports.
// Normally just used by buildLegacyLocations(), but also used by unit tests.
Server buildLegacyServer(const QString &address, const QString &serial,
                         Service service,
                         const std::vector<quint16> &allServicePorts);

// Build Location and Server objects for the legacy servers list from the
// legacy regions list and Shadowsocks list.
//
// Latencies are taken from the latency measurements map if available.
LocationsById buildLegacyLocations(const LatencyMap &latencies,
                                   const QJsonObject &serversObj,
                                   const QJsonObject &shadowsocksObj);

// Build the grouped and sorted locations from the flat locations.
std::vector<CountryLocations> buildGroupedLocations(const LocationsById &locations);

class NearestLocations
{
public:
    NearestLocations(const LocationsById &locations);

public:
    // Find the closest server location that is safe to use with 'connect auto'.
    // The safe servers are found in in the "auto_regions" server json. Note
    // that servers that do NOT appear in this list may still be connected to
    // manually.
    //
    // If the portForward parameter is true, then prefer the closest location
    // that supports port forwarding.  (Still fall back to the closest location
    // if none support PF.)
    QSharedPointer<Location> getNearestSafeVpnLocation(bool portForward) const;

    // Find the closest server location that is safe and satisfies an arbitrary
    // predicate.  Does _not_ fall back if no locations satisfy the predicate.
    // (Usually used to find a location that provides an auxiliary service, like
    // Shadowsocks.)
    template<class LocationTestFunc>
    QSharedPointer<Location> getNearestSafeServiceLocation(LocationTestFunc isAllowedLocation) const
    {
        auto itResult = std::find_if(_locations.begin(), _locations.end(),
            [&isAllowedLocation](const auto &pLocation)
            {
                return pLocation && pLocation->autoSafe() &&
                    isAllowedLocation(*pLocation);
            });
        if(itResult != _locations.end())
            return *itResult;
        return {};
    }

private:
    std::vector<QSharedPointer<Location>> _locations;
};

#endif
