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
#line SOURCE_FILE("locations.cpp")

#include "locations.h"
#include <QJsonDocument>

namespace
{
    QString addressHost(const QString &address)
    {
        // Returns the entire string if there is no colon for some reason
        return address.left(address.indexOf(':'));
    }

    quint16 addressPort(const QString &address)
    {
        auto colonIdx = address.indexOf(':');
        // Return 0 if the address didn't have a valid port somehow
        if(colonIdx < 0)
            return 0;

        // If the text after the colon isn't a valid uint16 (including if it's
        // empty), return 0.
        uint port = address.midRef(colonIdx+1).toUInt();
        if(port <= std::numeric_limits<quint16>::max())
            return static_cast<quint16>(port);
        return 0;
    }

    // The "geo" flag was added in legacy regions list v1002 and isn't yet
    // available in the modern regions list.  Tolerate issues when parsing this
    // - a missing flag is fine and defaults to false, any unexpected value is
    // also treated as false (with a warning).
    bool getGeoFlag(const QJsonObject &region, const QString &regionIdTrace)
    {
        if(region.contains("geo"))
        {
            try
            {
                return JsonCaster{region.value(QStringLiteral("geo"))};
            }
            catch(const Error &ex)
            {
                qWarning() << "Unable to read 'geo' flag of region"
                    << regionIdTrace << "-" << ex;
            }
        }

        return false;
    }
}

Server buildLegacyServer(const QString &address, const QString &serial,
                         Service service,
                         const std::vector<quint16> &allServicePorts)
{
    Server legacyServer;
    legacyServer.ip(addressHost(address));
    legacyServer.commonName(serial);

    // Build the service ports vector using the port from "address" as the
    // default, and all other ports from the global list (if any)
    std::vector<quint16> servicePorts;
    servicePorts.reserve(allServicePorts.size() + 1);
    quint16 defaultPort{addressPort(address)};
    if(defaultPort)
        servicePorts.push_back(defaultPort);
    for(quint16 port : allServicePorts)
    {
        if(port && port != defaultPort)
            servicePorts.push_back(port);
    }
    legacyServer.servicePorts(service, std::move(servicePorts));
    return legacyServer;
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
    const auto &shadowsocksServer = shadowsocksObj.value(locationId).toObject();
    if(!shadowsocksServer.isEmpty())
    {
        try
        {
            Server shadowsocks;
            shadowsocks.ip(JsonCaster{shadowsocksServer.value(QStringLiteral("host"))});
            // No serial, not provided (or used) for Shadowsocks
            shadowsocks.shadowsocksKey(JsonCaster{shadowsocksServer.value(QStringLiteral("key"))});
            shadowsocks.shadowsocksCipher(JsonCaster{shadowsocksServer.value(QStringLiteral("cipher"))});
            shadowsocks.shadowsocksPorts({json_cast<quint16>(shadowsocksServer.value(QStringLiteral("port")))});
            servers.push_back(std::move(shadowsocks));
        }
        catch(const Error &ex)
        {
            qWarning() << "Ignoring invalid Shadowsocks info for region"
                << locationId << "-" << ex;
        }
    }
}

LocationsById buildLegacyLocations(const LatencyMap &latencies,
                                   const QJsonObject &serversObj,
                                   const QJsonObject &shadowsocksObj)
{
    LocationsById newLocations;

    // The safe regions to be used with 'connect auto'
    std::vector<QString> safeAutoRegions{};
    try
    {
        safeAutoRegions = json_cast<std::vector<QString>>(serversObj["info"].toObject()["auto_regions"]);
    }
    catch (const Error &ex)
    {
        qWarning() << "Error loading json field auto_regions. Error:" << ex;
    }

    // The legacy servers list provides one global set of VPN ports, although
    // defaults are given for each server's endpoint.
    std::vector<quint16> ovpnUdpPorts, ovpnTcpPorts;

    try
    {
        const auto &portInfo = serversObj["info"].toObject()["vpn_ports"].toObject();
        ovpnUdpPorts = json_cast<std::vector<quint16>>(portInfo["udp"]);
        ovpnTcpPorts = json_cast<std::vector<quint16>>(portInfo["tcp"]);
    }
    catch (const Error &ex)
    {
        qWarning() << "Could not find supported vpn_ports from region data -"
            << ex;
    }


    // Each location is an attribute of the top-level JSON object.
    // Note that we can't use a range-based for loop over serversObj, since we
    // need its keys and values.  QJsonObject's iterator dereferences to just the
    // value, and the iterator itself has a key() method (unlike
    // std::map::iterator, for example, which dereferences to a pair).
    for(auto itAttr = serversObj.begin(); itAttr != serversObj.end(); ++itAttr)
    {
        // Ignore a few known extra fields to avoid spurious warnings, these are
        // not locations.
        if(itAttr.key() == QStringLiteral("web_ips") ||
           itAttr.key() == QStringLiteral("vpn_ports") ||
           itAttr.key() == QStringLiteral("info"))
        {
            continue;
        }

        // Create a ServerLocation and attempt to set all of its attributes.
        // If any attribute isn't present, isn't the right type, etc., JsonCaster
        // throws an exception.
        //
        // (We end up getting an Undefined QJsonValue when accessing the
        // attribute, and that can't be converted to the expected type of the
        // attribute in ServerLocation.  If the ServerLocation attribute is
        // Optional, an Undefined is fine and becomes a null.)
        try
        {
            QSharedPointer<Location> pLocation{new Location{}};

            // Is there a latency measurement for this location?
            auto itLatency = latencies.find(itAttr.key());
            if(itLatency != latencies.end())
            {
                // Apply the latency measurement.  (Otherwise, the latency is
                // left unset.)
                pLocation->latency(itLatency->second);
            }

            const auto &serverObj = itAttr.value().toObject();

            // The 'id' is actually the name of the attribute in the original data,
            // since they're stored as attributes of an object, not an array.
            pLocation->id(itAttr.key());
            pLocation->name(JsonCaster{serverObj.value(QStringLiteral("name"))});
            pLocation->country(JsonCaster{serverObj.value(QStringLiteral("country"))});
            pLocation->portForward(JsonCaster{serverObj.value(QStringLiteral("port_forward"))});
            pLocation->geoOnly(getGeoFlag(serverObj, itAttr.key()));
            // Set autoSafe from the list of auto regions.  Note that if all
            // regions have autoSafe() == false, we'll still be able to connect
            // automatically - NearestLocations::getNearestSafeVpnLocation()
            // will instead just take the nearest location since there were no
            // safe locations.
            pLocation->autoSafe(std::find(safeAutoRegions.begin(), safeAutoRegions.end(), pLocation->id()) != safeAutoRegions.end());

            // The legacy servers list just gives us one address (and default
            // port) per service.  Currently, the IP addresses are all the same,
            // but there's no need to rely on this when adapting to the new
            // model.  Create one Server per service endpoint.
            //
            // Latency is handled the same way, we create one Server object just
            // for the ping endpoint given.  This differs from the new servers
            // list, which provides latency on all servers to permit
            // per-server measurements, so Desktop handles both possibilities.
            std::vector<Server> servers;
            QString serial = JsonCaster{serverObj.value(QStringLiteral("serial"))};
            servers.push_back(buildLegacyServer(JsonCaster{serverObj.value(QStringLiteral("ping"))},
                                                serial, Service::Latency, {}));
            servers.push_back(buildLegacyServer(JsonCaster{serverObj.value(QStringLiteral("openvpn_udp")).toObject().value(QStringLiteral("best"))},
                                                serial, Service::OpenVpnUdp,
                                                ovpnUdpPorts));
            servers.push_back(buildLegacyServer(JsonCaster{serverObj.value(QStringLiteral("openvpn_tcp")).toObject().value(QStringLiteral("best"))},
                                                serial, Service::OpenVpnTcp,
                                                ovpnTcpPorts));
            const auto &wireguardServer = serverObj.value(QStringLiteral("wireguard")).toObject();
            if(!wireguardServer.isEmpty())
            {
                servers.push_back(buildLegacyServer(JsonCaster{wireguardServer.value(QStringLiteral("host"))},
                                                    JsonCaster{wireguardServer.value(QStringLiteral("serial"))},
                                                    Service::WireGuard, {}));
            }
            else
            {
                // This is tolerated but not completely supported by the client,
                // automatic region selection / UI / etc. won't handle this
                // fully correctly.
                qWarning() << "Region does not support WireGuard -" << pLocation->id();
            }

            // Look for Shadowsocks info for this region
            addLegacyShadowsocksServer(servers, pLocation->id(), shadowsocksObj);

            pLocation->servers(servers);

            // This location is good, store it
            newLocations[itAttr.key()] = std::move(pLocation);
        }
        catch(const Error &ex)
        {
            qWarning() << "Can't load location" << itAttr.key()
                << "due to error" << ex;
        }
    }

    return newLocations;
}

void applyModernService(const QJsonObject &serviceObj, Server &groupTemplate,
                        const QString &groupTrace)
{
    try
    {
        const auto &serviceName = json_cast<QString>(serviceObj["name"]);
        Service knownService = Service::Latency;
        if(serviceName == QStringLiteral("openvpn_tcp"))
            knownService = Service::OpenVpnTcp;
        else if(serviceName == QStringLiteral("openvpn_udp"))
            knownService = Service::OpenVpnUdp;
        else if(serviceName == QStringLiteral("wireguard"))
            knownService = Service::WireGuard;
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
        auto servicePorts = json_cast<std::vector<quint16>>(serviceObj["ports"]);
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
        newServer.ip(JsonCaster{serverObj["ip"]});
        newServer.commonName(JsonCaster{serverObj["cn"]});
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
        pLocation->id(JsonCaster{regionObj["id"]});
        id = pLocation->id();   // Found an id, trace it if the location fails
        pLocation->name(JsonCaster{regionObj["name"]});
        pLocation->country(JsonCaster{regionObj["country"]});
        pLocation->geoOnly(getGeoFlag(regionObj, pLocation->id()));
        pLocation->autoSafe(JsonCaster{regionObj["auto_region"]});
        pLocation->portForward(JsonCaster{regionObj["port_forward"]});

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

    // If the location was loaded but has no servers, treat that as an error
    // too.
    if(pLocation && pLocation->servers().empty())
    {
        qWarning() << "Location" << pLocation->id() << "has no servers, ignored";
        return {};
    }

    return pLocation;
}

LocationsById buildModernLocations(const LatencyMap &latencies,
                                   const QJsonObject &regionsObj,
                                   const QJsonObject &legacyShadowsocksObj)
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
    for(const auto &regionValue : regionsArray)
    {
        const auto &regionObj = regionValue.toObject();

        auto pLocation = readModernLocation(regionObj, groupTemplates, legacyShadowsocksObj);

        if(pLocation)
        {
            // Is there a latency measurement for this location?
            auto itLatency = latencies.find(pLocation->id());
            if(itLatency != latencies.end())
            {
                // Apply the latency measurement.  (Otherwise, the latency is
                // left unset.)
                pLocation->latency(itLatency->second);
            }
            newLocations[pLocation->id()] = std::move(pLocation);
        }
        // Failure to load the location is traced by readModernLocation()
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

std::vector<CountryLocations> buildGroupedLocations(const LocationsById &locations)
{
    // Group the locations by country
    std::unordered_map<QString, std::vector<QSharedPointer<Location>>> countryGroups;

    for(const auto &locationEntry : locations)
    {
        Q_ASSERT(locationEntry.second);
        countryGroups[locationEntry.second->country().toLower()].push_back(locationEntry.second);
    }

    // Sort each countries' locations by latency, then id
    for(auto &group : countryGroups)
    {
        std::sort(group.second.begin(), group.second.end(),
            [](const auto &pFirst, const auto &pSecond)
            {
                Q_ASSERT(pFirst);
                Q_ASSERT(pSecond);

                return compareEntries(*pFirst, *pSecond);
            });
    }

    // Create country groups from the sorted lists
    std::vector<CountryLocations> countries;
    countries.reserve(countryGroups.size());
    for(const auto &group : countryGroups)
    {
        countries.push_back({});
        countries.back().locations(group.second);
    }

    // Sort the countries by their lowest latency
    std::sort(countries.begin(), countries.end(),
        [](const auto &first, const auto &second)
        {
            // Consequence of above; countries created with at least 1 location
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

    return countries;
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
