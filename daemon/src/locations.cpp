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
                                                    serial, Service::WireGuard, {}));
            }
            else
            {
                // This is tolerated but not completely supported by the client,
                // automatic region selection / UI / etc. won't handle this
                // fully correctly.
                qWarning() << "Region does not support WireGuard -" << pLocation->id();
            }

            // Look for Shadowsocks info for this region
            const auto &shadowsocksServer = shadowsocksObj.value(pLocation->id()).toObject();
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
                        << pLocation->id() << "-" << ex;
                }
            }

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
    if(_locations.empty())
    {
        qWarning() << "There are no available Server Locations!";
        return {};
    }

    // If port forwarding is on, then find fastest server that supports port forwarding
    if(portForward)
    {
        auto result = std::find_if(_locations.begin(), _locations.end(),
            [](const auto &location)
            {
                return location->portForward() && location->autoSafe();
            });
        if(result != _locations.end())
            return *result;
    }

    // otherwise just find the fastest 'safe' server
    auto result = std::find_if(_locations.begin(), _locations.end(),
        [](const auto &location) {return location->autoSafe();});
    if(result != _locations.end())
        return *result;

    // We fall-back to the fastest region since we could not find a region meeting the above constraints
    qWarning() << "Unable to find closest server location meeting constraints, falling back to fastest region";
    return _locations.front();
}
