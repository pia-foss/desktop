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

#include "common.h"
#line SOURCE_FILE("locations.cpp")

#include "locations.h"
#include <kapps_regions/src/regionlist.h>
#include <kapps_regions/src/metadata.h>
#include <QJsonDocument>

namespace
{
    // Constants used for the manual region if one is created
    const kapps::core::StringSlice manualRegionId{"manual"};
    // Although service groups can be changed at any time by Ops (these are just
    // intended to abbreviate the regions list), defaults are present for a
    // manual region, as this is a dev tool only.  If the groups change, the
    // sevice groups can be specified in the override.
    const std::vector<QString> manualRegionDefaultGroups{
        QStringLiteral("ovpntcp"),
        QStringLiteral("ovpnudp"),
        QStringLiteral("wg")
    };
}

const std::unordered_map<QString, QString> shadowsocksLegacyRegionMap
{
    {QStringLiteral("us_south_west"), QStringLiteral("us_dal")},
    {QStringLiteral("us_seattle"), QStringLiteral("us_sea")},
    {QStringLiteral("us-newjersey"), QStringLiteral("us_nyc")}, // "US East"
    {QStringLiteral("japan"), QStringLiteral("jp")},
    {QStringLiteral("uk-london"), QStringLiteral("uk")},
    {QStringLiteral("nl_amsterdam"), QStringLiteral("nl")}
};

auto buildModernLocations(const LatencyMap &latencies,
                          const QJsonObject &regionsObj,
                          const QJsonArray &shadowsocksObj,
                          const QJsonObject &metadataObj,
                          const std::vector<AccountDedicatedIp> &dedicatedIps,
                          const ManualServer &manualServer)
    -> std::pair<LocationsById, kapps::regions::Metadata>
{
    QByteArray regionsJson = QJsonDocument{regionsObj}.toJson();
    QByteArray shadowsocksJson = QJsonDocument{shadowsocksObj}.toJson();
    QByteArray metadataJson = QJsonDocument{metadataObj}.toJson();
    kapps::core::StringSlice regionsJsonSlice{regionsJson.data(),
        static_cast<std::size_t>(regionsJson.size())};
    kapps::core::StringSlice shadowsocksJsonSlice{shadowsocksJson.data(),
        static_cast<std::size_t>(shadowsocksJson.size())};
    kapps::core::StringSlice metadataJsonSlice{metadataJson.data(),
        static_cast<std::size_t>(metadataJson.size())};

    // We can't reference QString data with a StringSlice because QString is
    // UTF-16, we have to convert them
    std::deque<std::string> convertedStrings;
    auto qstrSlice = [&](const QString &qstr) -> kapps::core::StringSlice
    {
        convertedStrings.push_back(qstr.toStdString());
        return convertedStrings.back();
    };
    // Similarly with the arrays of service groups
    std::deque<std::vector<kapps::core::StringSlice>> convertedServiceGroups;
    auto serviceGroupsSlice = [&](const std::vector<QString> &serviceGroups)
        -> kapps::core::ArraySlice<const kapps::core::StringSlice>
    {
        convertedServiceGroups.push_back({});
        convertedServiceGroups.back().reserve(serviceGroups.size());
        for(const auto &groupName : serviceGroups)
            convertedServiceGroups.back().push_back(qstrSlice(groupName));
        return convertedServiceGroups.back();
    };
    std::vector<kapps::regions::DedicatedIp> dips;
    dips.reserve(dedicatedIps.size());
    for(const auto &accountDip : dedicatedIps)
    {
        dips.push_back({qstrSlice(accountDip.id()),
                        kapps::core::Ipv4Address{accountDip.ip().toStdString()},
                        qstrSlice(accountDip.cn()),
                        {}, // FQDN is not used in PIA
                        serviceGroupsSlice(accountDip.serviceGroups()),
                        qstrSlice(accountDip.regionId())});
    }

    std::vector<kapps::regions::ManualRegion> manual;
    manual.reserve(1);
    if(!manualServer.ip().isEmpty() && !manualServer.cn().isEmpty())
    {
        const std::vector<QString> &groups = manualServer.serviceGroups().empty() ?
            manualRegionDefaultGroups : manualServer.serviceGroups();
        manual.push_back({manualRegionId,
                          kapps::core::Ipv4Address{manualServer.ip().toStdString()},
                          qstrSlice(manualServer.cn()),
                          {},   // FQDN is not used in PIA
                          serviceGroupsSlice(groups),
                          qstrSlice(manualServer.correspondingRegionId()),
                          manualServer.openvpnNcpSupport(),
                          manualServer.openvpnUdpPorts(),
                          manualServer.openvpnTcpPorts()});
    }

    kapps::regions::RegionList regionlist{kapps::regions::RegionList::PIAv6,
                                          regionsJsonSlice, shadowsocksJsonSlice,
                                          dips, manual};
    kapps::regions::Metadata metadata{regionsJsonSlice, metadataJsonSlice,
                                      dips, manual};

    LocationsById newLocations;
    for(const auto &pRegion : regionlist.regions())
    {
        if(!pRegion)
            continue;
        QString regionId{qs::toQString(pRegion->id())};
        nullable_t<double> latency;
        auto itLatency = latencies.find(regionId);
        if(itLatency != latencies.end())
            latency.emplace(itLatency->second);

        newLocations.emplace(pRegion->id().to_string(),
            QSharedPointer<Location>::create(pRegion->shared_from_this(), latency));
    }

    return {std::move(newLocations), std::move(metadata)};
}

// Compare two locations to sort them.
// Sorts by latencies first, then by IDs.
// Tiebreaking by ID ensures that we sort regions the same way in all contexts.
bool compareEntries(const Location &first, const Location &second)
{
    const Optional<double> &firstLatency = first.latency();
    const Optional<double> &secondLatency = second.latency();
    // Unknown latencies sort last
    if(firstLatency && !secondLatency)
        return true;    // *this < other
    if(!firstLatency && secondLatency)
        return false;   // *this > other

    // If the latencies are known and different, compare them
    if(firstLatency && firstLatency.get() != secondLatency.get())
        return firstLatency.get() < secondLatency.get();

    // Otherwise, the latencies are equivalent (both known and
    // equal, or both unknown and unequal)
    // Compare IDs.
    return first.id().compare(second.id(), Qt::CaseSensitivity::CaseInsensitive) < 0;
}

void buildGroupedLocations(const LocationsById &locations,
                           const kapps::regions::Metadata &metadata,
                           std::vector<CountryLocations> &groupedLocations,
                           std::vector<QSharedPointer<const Location>> &dedicatedIpLocations)
{
    // Group the locations by country
    std::unordered_map<kapps::core::StringSlice, std::vector<QSharedPointer<const Location>>> countryGroups;
    dedicatedIpLocations.clear();

    for(const auto &locationEntry : locations)
    {
        Q_ASSERT(locationEntry.second);
        if(locationEntry.second->isDedicatedIp())
            dedicatedIpLocations.push_back(locationEntry.second);
        else
        {
            kapps::core::StringSlice countryCode;
            auto pRegionDisplay = metadata.getRegionDisplay(locationEntry.second->id().toStdString());
            if(pRegionDisplay)
                countryCode = pRegionDisplay->country();
            countryGroups[countryCode].push_back(locationEntry.second);
        }
    }

    // Sort each countries' locations by latency, then id
    auto sortLocations = [](const QSharedPointer<const Location> &pFirst,
                             const QSharedPointer<const Location> &pSecond)
    {
        Q_ASSERT(pFirst);
        Q_ASSERT(pSecond);

        return compareEntries(*pFirst, *pSecond);
    };

    for(auto &group : countryGroups)
    {
        std::sort(group.second.begin(), group.second.end(), sortLocations);
    }

    // Sort dedicated IP locations in the same way
    std::sort(dedicatedIpLocations.begin(), dedicatedIpLocations.end(), sortLocations);

    // Create country groups from the sorted lists
    groupedLocations.clear();
    groupedLocations.reserve(countryGroups.size());
    for(auto &group : countryGroups)
    {
        groupedLocations.emplace_back(group.first.to_string(),
            std::move(group.second));
    }

    // Sort the countries by their lowest latency
    std::sort(groupedLocations.begin(), groupedLocations.end(),
        [](const auto &first, const auto &second)
        {
            // Consequence of above; groupedLocations created with at least 1 location
            Q_ASSERT(!first.locations().empty());
            Q_ASSERT(!second.locations().empty());
            // Sort by the lowest latency for each country, then region ID if
            // the latencies are the same
            const auto &pFirstNearest = first.locations().front();
            const auto &pSecondNearest = second.locations().front();

            Q_ASSERT(pFirstNearest);
            Q_ASSERT(pSecondNearest);
            return compareEntries(*pFirstNearest, *pSecondNearest);
        });
}

NearestLocations::NearestLocations(const LocationsById &allLocations)
{
    _locations.reserve(allLocations.size());
    for(const auto &locationEntry : allLocations)
        _locations.push_back(locationEntry.second);
    std::sort(_locations.begin(), _locations.end(),
                   [](const auto &pFirst, const auto &pSecond) {
                       Q_ASSERT(pFirst);
                       Q_ASSERT(pSecond);

                       return compareEntries(*pFirst, *pSecond);
                   });
}

QSharedPointer<const Location> NearestLocations::getNearestSafeVpnLocation(bool portForward) const
{
    // If port forwarding is on, then find fastest server that supports port forwarding
    if(portForward)
    {
        auto result = getBestMatchingLocation([](const Location &loc){return loc.portForward();});
        if(result)
            return result;
    }

    // otherwise just find the best non-PF server
    return getBestLocation();
}
