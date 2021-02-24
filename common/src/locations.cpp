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
#line SOURCE_FILE("locations.cpp")

#include "locations.h"
#include <QJsonDocument>

namespace
{
    // The "geo" flag was added in legacy regions list v1002 and isn't yet
    // available in the modern regions list.  Tolerate issues when parsing this
    // - a missing flag is fine and defaults to false, any unexpected value is
    // also treated as false (with a warning).
    bool getOptionalFlag(const QString &flagName, const QJsonObject &region, const QString &regionIdTrace)
    {
        if(region.contains(flagName))
        {
            try
            {
                return json_cast<bool>(region.value(flagName), HERE);
            }
            catch(const Error &ex)
            {
                qWarning() << QStringLiteral("Unable to read %1 flag of region").arg(flagName)
                    << regionIdTrace << "-" << ex;
            }
        }

        return false;
    }
}


// If the location ID given is present in the Shadowsocks region data, add a
// Shadowsocks server to the list of servers for this location.
//
// If the location ID doesn't have Shadowsocks, nothing happens.  If the
// Shadowsocks server data are invalid, the error is traced and no changes are
// made.
void addLegacyShadowsocksServer(std::vector<Server> &servers,
                                const QString &locationId,
                                const QJsonObject &shadowsocksObj)
{
    // Look for Shadowsocks info for this region
    QJsonObject shadowsocksServer = shadowsocksObj.value(locationId).toObject();

    // The Shadowsocks region list still uses legacy location IDs, map some of
    // the next-gen location IDs to legacy IDs where they differ
    if(shadowsocksServer.isEmpty())
    {
        if(locationId == QStringLiteral("us_south_west"))
            shadowsocksServer = shadowsocksObj.value("us_dal").toObject();
        else if(locationId == QStringLiteral("us_seattle"))
            shadowsocksServer = shadowsocksObj.value("us_sea").toObject();
        else if(locationId == QStringLiteral("us-newjersey"))   // "US East"
            shadowsocksServer = shadowsocksObj.value("us_nyc").toObject();
        else if(locationId == QStringLiteral("japan"))
            shadowsocksServer = shadowsocksObj.value("jp").toObject();
    }

    if(!shadowsocksServer.isEmpty())
    {
        try
        {
            Server shadowsocks;
            shadowsocks.ip(json_cast<QString>(shadowsocksServer.value(QStringLiteral("host")), HERE));
            // No serial, not provided (or used) for Shadowsocks
            shadowsocks.shadowsocksKey(json_cast<QString>(shadowsocksServer.value(QStringLiteral("key")), HERE));
            shadowsocks.shadowsocksCipher(json_cast<QString>(shadowsocksServer.value(QStringLiteral("cipher")), HERE));
            shadowsocks.shadowsocksPorts({json_cast<quint16>(shadowsocksServer.value(QStringLiteral("port")), HERE)});
            servers.push_back(std::move(shadowsocks));
        }
        catch(const Error &ex)
        {
            qWarning() << "Ignoring invalid Shadowsocks info for region"
                << locationId << "-" << ex;
        }
    }
}

void applyModernService(const QJsonObject &serviceObj, Server &groupTemplate,
                        const QString &groupTrace)
{
    try
    {
        const auto &serviceName = json_cast<QString>(serviceObj["name"], HERE);
        Service knownService = Service::Latency;
        if(serviceName == QStringLiteral("openvpn_tcp"))
            knownService = Service::OpenVpnTcp;
        else if(serviceName == QStringLiteral("openvpn_udp"))
            knownService = Service::OpenVpnUdp;
        else if(serviceName == QStringLiteral("wireguard"))
            knownService = Service::WireGuard;
        else if(serviceName == QStringLiteral("meta"))
            knownService = Service::Meta;
        else if(serviceName == QStringLiteral("latency"))
            knownService = Service::Latency;
        else
        {
            // Otherwise, some other service not used by Desktop - ignore silently
            // Shadowsocks is ignored in the new servers list; we haven't
            // determined how to provide the Shadowsocks key/cipher yet.
            return;
        }
        // Don't load "ports" until we know it's a service we use, some future
        // services might not have ports in the same way.
        auto servicePorts = json_cast<std::vector<quint16>>(serviceObj["ports"], HERE);
        groupTemplate.servicePorts(knownService, servicePorts);
    }
    catch(const Error &ex)
    {
        qWarning() << "Service in group" << groupTrace << "is not valid:" << ex
            << QJsonDocument{serviceObj}.toJson();
    }
}

void readModernServer(const Server &groupTemplate, std::vector<Server> &servers,
                      const QJsonObject &serverObj, const QString &rgnIdTrace)
{
    try
    {
        Server newServer{groupTemplate};
        newServer.ip(json_cast<QString>(serverObj["ip"], HERE));
        newServer.commonName(json_cast<QString>(serverObj["cn"], HERE));
        // TODO - Need Shadowsocks key/cipher for servers with Shadowsocks
        // service
        servers.push_back(newServer);
    }
    catch(const Error &ex)
    {
        qWarning() << "Can't load server in location" << rgnIdTrace
            << "due to error:" << ex;
    }
}

QSharedPointer<Location> readModernLocation(const QJsonObject &regionObj,
                                            const std::unordered_map<QString, Server> &groupTemplates,
                                            const QJsonObject &legacyShadowsocksObj)
{
    QSharedPointer<Location> pLocation{new Location{}};
    QString id; // For tracing, if we get an ID and the read fails, this will be traced
    try
    {
        pLocation->id(json_cast<QString>(regionObj["id"], HERE));
        id = pLocation->id();   // Found an id, trace it if the location fails
        pLocation->name(json_cast<QString>(regionObj["name"], HERE));
        pLocation->country(json_cast<QString>(regionObj["country"], HERE));
        pLocation->geoOnly(getOptionalFlag(QStringLiteral("geo"), regionObj, pLocation->id()));
        pLocation->autoSafe(json_cast<bool>(regionObj["auto_region"], HERE));
        pLocation->portForward(json_cast<bool>(regionObj["port_forward"], HERE));
        pLocation->autoSafe(json_cast<bool>(regionObj["auto_region"], HERE));
        pLocation->portForward(json_cast<bool>(regionObj["port_forward"], HERE));
        pLocation->offline(getOptionalFlag("offline", regionObj, pLocation->id()));
        // Build servers
        std::vector<Server> servers;
        const auto &serverGroupsObj = regionObj["servers"].toObject();
        auto itGroup = serverGroupsObj.begin();
        while(itGroup != serverGroupsObj.end())
        {
            // Find the group template
            auto itTemplate = groupTemplates.find(itGroup.key());
            if(itTemplate == groupTemplates.end())
            {
                qWarning() << "Group" << itGroup.key() << "not known in location"
                    << pLocation->id();
                // Skip all servers in this group
            }
            // If the group template has no known services, skip this group
            // silently.  This can be normal for services not used by Desktop.
            else if(itTemplate->second.hasNonLatencyService())
            {
                for(const auto &serverValue : itGroup->toArray())
                {
                    const auto &serverObj = serverValue.toObject();
                    readModernServer(itTemplate->second, servers, serverObj,
                        pLocation->id());
                }
            }
            ++itGroup;
        }

        // Include the legacy Shadowsocks server for this location if present
        addLegacyShadowsocksServer(servers, pLocation->id(), legacyShadowsocksObj);

        pLocation->servers(servers);
    }
    catch(const Error &ex)
    {
        qWarning() << "Can't load location" << id << "due to error" << ex;
        return {};
    }

    // If the location was loaded but has no servers, treat that as if the location
    // is offline
    if(pLocation && pLocation->servers().empty())
    {
        pLocation->offline(true);
        qWarning() << "Location" << pLocation->id() << "has no servers, setting as offline";
    }

    return pLocation;
}

// Build a Location from a dedicated IP.  Location metadata (name, country,
// geo, PF, etc.) are taken from the corresponding normal location.
QSharedPointer<Location> buildDedicatedIpLocation(const LocationsById &modernLocations,
                                                  const std::unordered_map<QString, Server> &groupTemplates,
                                                  const AccountDedicatedIp &dip)
{
    QSharedPointer<Location> pLocation{new Location{}};

    // Set up the essential parts (ID and servers) of the location that we know
    // even if the corresponding location is not found for some reason.
    pLocation->id(dip.id());
    // DIP locations are never selected automatically
    pLocation->autoSafe(false);
    pLocation->dedicatedIp(dip.ip());
    pLocation->dedicatedIpExpire(dip.expire());
    pLocation->dedicatedIpCorrespondingRegion(dip.regionId());
    std::vector<Server> servers;
    for(const auto &group : dip.serviceGroups())
    {
        // Find the group template
        auto itTemplate = groupTemplates.find(group);
        if(itTemplate == groupTemplates.end())
        {
            qWarning() << "Group" << group << "not known in location"
                << pLocation->id();
            // Skip this group
        }
        // Ignore groups that have no services used by Desktop, this can be
        // normal if it had other services that we don't care about
        else if(itTemplate->second.hasNonLatencyService())
        {
            // Create a server for this group using the DIP IP/CN
            Server newServer{itTemplate->second};
            newServer.ip(dip.ip());
            newServer.commonName(dip.cn());
            servers.push_back(std::move(newServer));
        }
    }

    // Try to find the corresponding location for metadata
    auto itCorrespondingLocation = modernLocations.find(dip.regionId());
    if(itCorrespondingLocation == modernLocations.end())
    {
        // Couldn't find the location - set defaults.  The DIP region will still
        // be available, but without country/name info
        pLocation->name({});
        pLocation->country({});
        pLocation->portForward(false);
        pLocation->geoOnly(false);
    }
    else
    {
        pLocation->name(itCorrespondingLocation->second->name());
        pLocation->country(itCorrespondingLocation->second->country());
        pLocation->portForward(itCorrespondingLocation->second->portForward());
        pLocation->geoOnly(itCorrespondingLocation->second->geoOnly());

        // Use the 'meta' service from server(s) in the corresponding location,
        // but not any other services
        for(const auto &correspondingServer : itCorrespondingLocation->second->servers())
        {
            if(correspondingServer.hasService(Service::Meta))
            {
                // Create a new server and copy over just the 'meta' info; do
                // not take any other services that might be offered on this
                // server.
                Server metaServer{};
                metaServer.ip(correspondingServer.ip());
                metaServer.commonName(correspondingServer.commonName());
                metaServer.metaPorts(correspondingServer.metaPorts());
                servers.push_back(std::move(metaServer));
            }
        }
    }

    pLocation->servers(std::move(servers));
    return pLocation;
}

void applyLatency(Location &location, const LatencyMap &latencies)
{
    // Is there a latency measurement for this location?
    auto itLatency = latencies.find(location.id());
    if(itLatency != latencies.end())
    {
        // Apply the latency measurement.  (Otherwise, the latency is
        // left unset.)
        location.latency(itLatency->second);
    }
}

LocationsById buildModernLocations(const LatencyMap &latencies,
                                   const QJsonObject &regionsObj,
                                   const QJsonObject &legacyShadowsocksObj,
                                   const std::vector<AccountDedicatedIp> &dedicatedIps)
{
    // Build template Server objects for each "group" given in the regions list.
    // These will be used later to construct the actual servers by filling in
    // an ID and common name.
    std::unordered_map<QString, Server> groupTemplates;
    const auto &groupsObj = regionsObj["groups"].toObject();

    // Group names are in keys, use Qt iterators
    auto itGroup = groupsObj.begin();
    while(itGroup != groupsObj.end())
    {
        Server groupTemplate;
        // Apply the services.  There may be other services that Desktop doesn't
        // use, ignore those.
        for(const auto &service : itGroup.value().toArray())
            applyModernService(service.toObject(), groupTemplate, itGroup.key());

        // Keep groups even if they have no known services.  This prevents
        // spurious "unknown group" warnings, the servers in this group will be
        // ignored.
        groupTemplates[itGroup.key()] = std::move(groupTemplate);
        ++itGroup;
    }

    // Now read the locations and use the group templates to build servers
    LocationsById newLocations;
    const auto &regionsArray = regionsObj["regions"].toArray();
    int regionsLackingWireguard = 0;    // For tracing
    for(const auto &regionValue : regionsArray)
    {
        const auto &regionObj = regionValue.toObject();

        auto pLocation = readModernLocation(regionObj, groupTemplates, legacyShadowsocksObj);

        if(pLocation)
        {
            applyLatency(*pLocation, latencies);

            if(!pLocation->hasService(Service::WireGuard))
                ++regionsLackingWireguard;

            newLocations[pLocation->id()] = std::move(pLocation);
        }
        // Failure to load the location is traced by readModernLocation()
    }

    if(regionsLackingWireguard > 0)
    {
        qWarning() << "Found" << regionsLackingWireguard
            << "regions with no WireGuard endpoints:"
            << QJsonDocument{regionsObj}.toJson();
    }

    // Build dedicated IP regions
    for(const auto &dip : dedicatedIps)
    {
        auto pLocation = buildDedicatedIpLocation(newLocations, groupTemplates, dip);
        if(pLocation)
        {
            applyLatency(*pLocation, latencies);
            newLocations[pLocation->id()] = std::move(pLocation);
        }
    }

    return newLocations;
}

// Compare two locations or countries to sort them.
// Sorts by latencies first, then country codes, then by IDs.
// The "tiebreaking" fields (country codes / IDs) are fixed to ensure that we
// sort regions the same way in all contexts.
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
    // Compare country codes.
    auto countryComparison = first.country().compare(second.country(),
                                                     Qt::CaseSensitivity::CaseInsensitive);
    if(countryComparison != 0)
        return countryComparison < 0;

    // Same latency and country, compare IDs.
    return first.id().compare(second.id(), Qt::CaseSensitivity::CaseInsensitive) < 0;
}

void buildGroupedLocations(const LocationsById &locations,
                           std::vector<CountryLocations> &groupedLocations,
                           std::vector<QSharedPointer<Location>> &dedicatedIpLocations)
{
    // Group the locations by country
    std::unordered_map<QString, std::vector<QSharedPointer<Location>>> countryGroups;
    dedicatedIpLocations.clear();

    for(const auto &locationEntry : locations)
    {
        Q_ASSERT(locationEntry.second);
        if(locationEntry.second->isDedicatedIp())
            dedicatedIpLocations.push_back(locationEntry.second);
        else
        {
            const auto &countryCode = locationEntry.second->country().toLower();
            countryGroups[countryCode].push_back(locationEntry.second);
        }
    }

    // Sort each countries' locations by latency, then id
    auto sortLocations = [](const QSharedPointer<Location> &pFirst,
                            const QSharedPointer<Location> &pSecond)
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
    for(const auto &group : countryGroups)
    {
        groupedLocations.push_back({});
        groupedLocations.back().locations(group.second);
    }

    // Sort the countries by their lowest latency
    std::sort(groupedLocations.begin(), groupedLocations.end(),
        [](const auto &first, const auto &second)
        {
            // Consequence of above; groupedLocations created with at least 1 location
            Q_ASSERT(!first.locations().empty());
            Q_ASSERT(!second.locations().empty());
            // Sort by the lowest latency for each country, then country code if
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

QSharedPointer<Location> NearestLocations::getNearestSafeVpnLocation(bool portForward) const
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
