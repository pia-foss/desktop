// Copyright (c) 2019 London Trust Media Incorporated
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
#line SOURCE_FILE("settings.cpp")

#include "settings.h"
#include "brand.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSharedPointer>
#include <iterator>

QString ServerLocation::addressHost(const QString &address)
{
    // Returns the entire string if there is no colon for some reason
    return address.left(address.indexOf(':'));
}

quint16 ServerLocation::addressPort(const QString &address)
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

void Transport::resolvePort(const ServerLocation &location)
{
    if(port() != 0)
        return;

    // If the server location didn't have a default port for some reason, the
    // port is still 0.
    if(protocol() == QStringLiteral("tcp"))
        port(location.tcpPort());
    else
        port(location.udpPort());
}

DaemonData::DaemonData()
    : NativeJsonObject(DiscardUnknownProperties)
{

}

DaemonAccount::DaemonAccount()
    : NativeJsonObject(DiscardUnknownProperties)
{

}

SplitTunnelRule::SplitTunnelRule()
    : NativeJsonObject(DiscardUnknownProperties)
{

}

DaemonSettings::DaemonSettings()
    : NativeJsonObject(SaveUnknownProperties)
{
}

DaemonState::DaemonState()
    : NativeJsonObject(DiscardUnknownProperties)
{
    // If any property of a ServerLocations object changes, consider the
    // ServerLocations property changed also.  (Daemon does not monitor for
    // nested property changes.)
    auto connectServiceLocations = [this](ServiceLocations &locs, auto func)
    {
        connect(&locs, &ServiceLocations::chosenLocationChanged, this, func);
        connect(&locs, &ServiceLocations::bestLocationChanged, this, func);
        connect(&locs, &ServiceLocations::nextLocationChanged, this, func);
    };

    connectServiceLocations(_vpnLocations, [this](){
        emitPropertyChange({[this](){emit vpnLocationsChanged();},
                            QStringLiteral("vpnLocations")});
    });
    connectServiceLocations(_shadowsocksLocations, [this](){
        emitPropertyChange({[this](){emit shadowsocksLocationsChanged();},
                            QStringLiteral("shadowsocksLocations")});
    });

    auto connectConnectionInfo = [this](ConnectionInfo &info, auto func)
    {
        connect(&info, &ConnectionInfo::vpnLocationChanged, this, func);
        connect(&info, &ConnectionInfo::vpnLocationAutoChanged, this, func);
        connect(&info, &ConnectionInfo::proxyChanged, this, func);
        connect(&info, &ConnectionInfo::proxyCustomChanged, this, func);
        connect(&info, &ConnectionInfo::proxyShadowsocksChanged, this, func);
        connect(&info, &ConnectionInfo::proxyShadowsocksLocationAutoChanged, this, func);
    };

    connectConnectionInfo(_connectingConfig, [this](){
        emitPropertyChange({[this](){emit connectingConfigChanged();},
                            QStringLiteral("connectingConfig")});
    });
    connectConnectionInfo(_connectedConfig, [this](){
        emitPropertyChange({[this](){emit connectedConfigChanged();},
                            QStringLiteral("connectedConfig")});
    });
}

QStringList DaemonData::getCertificateAuthority(const QString& type)
{
    auto it = certificateAuthorities().find(type);
    if (it != certificateAuthorities().end())
        return *it;
    qWarning() << "Unable to find certificate authority" << type;
    return certificateAuthorities().value(QStringLiteral("default"));
}

bool DaemonAccount::validateToken(const QString& token)
{
    static const QRegularExpression validToken(QStringLiteral("^[0-9A-Fa-f]+$"));
    return token.isEmpty() || validToken.match(token).hasMatch();
}

const QString DaemonSettings::defaultReleaseChannelGA{QStringLiteral(BRAND_RELEASE_CHANNEL_GA)};
const QString DaemonSettings::defaultReleaseChannelBeta{QStringLiteral(BRAND_RELEASE_CHANNEL_BETA)};
const QStringList DaemonSettings::defaultDebugLogging
{
    QStringLiteral("*.debug=true"),
    QStringLiteral("qt*.debug=false"),
    QStringLiteral("latency.*=false"),
    QStringLiteral("qt.scenegraph.general*=true")
};

QJsonValue DaemonSettings::getDefaultDebugLogging()
{
    QJsonValue value;
    json_cast(defaultDebugLogging, value);
    return value;
}

bool DaemonSettings::validateDNSSetting(const DaemonSettings::DNSSetting& setting)
{
    static const QStringList validDNSSettings {
        "pia",
        "handshake",
    };
    static const QRegularExpression validIP(QStringLiteral("^(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)$"));
    QString value;
    if (setting.get(value))
        return value.isEmpty() || validDNSSettings.contains(value);
    QStringList servers;
    if (setting.get(servers))
    {
        if (servers.isEmpty() || servers.length() > 2)
            return false;
        for (const QString& server : servers)
        {
            auto match = validIP.match(server);
            if (!match.hasMatch())
                return false;
            for (int i = 1; i <= 4; i++)
            {
                bool ok;
                if (match.captured(i).toUInt(&ok) > 255 || !ok)
                    return false;
            }
        }
        return true;
    }
    return false;
}

#if defined(PIA_DAEMON) || defined(UNIT_TEST)

ServerLocations updateServerLocations(const ServerLocations &existingLocations,
                                      const QJsonObject &serversObj)
{
    ServerLocations newLocations;

    // The safe regions to be used with 'connect auto'
    QVector<QString> safeAutoRegions{};
    try
    {
        safeAutoRegions = JsonCaster{serversObj["info"].toObject()["auto_regions"]};
    }
    catch (const std::exception &ex)
    {
        qWarning() << "Error loading json field auto_regions. Error:" << ex.what();
    }

    //Each location is an attribute of the top-level JSON object.
    //Note that we can't use a range-based for loop over serversObj, since we
    //need its keys and values.  QJsonObject's iterator dereferences to just the
    //value, and the iterator itself has a key() method (unlike
    //std::map::iterator, for example, which dereferences to a pair).
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

        //Create a ServerLocation and attempt to set all of its attributes.
        //If any attribute isn't present, isn't the right type, etc., JsonCaster
        //throws an exception.
        //
        //(We end up getting an Undefined QJsonValue when accessing the
        //attribute, and that can't be converted to the expected type of the
        //attribute in ServerLocation.  If the ServerLocation attribute is
        //Optional, an Undefined is fine and becomes a null.)
        try
        {
            QSharedPointer<ServerLocation> pLocation;
            //Does this location already exist?
            auto itExisting = existingLocations.find(itAttr.key());
            if(itExisting != existingLocations.end())
            {
                //Preserve the properties from the existing location.
                //This copies the properties that we're about to overwrite too,
                //but it's more maintainable to avoid enumerating the other
                //properties of ServerLocation here.
                pLocation.reset(new ServerLocation{**itExisting});
            }
            else
            {
                pLocation.reset(new ServerLocation{});
            }

            const auto &serverObj = itAttr.value().toObject();

            //The 'id' is actually the name of the attribute in the original data,
            //since they're stored as attributes of an object, not an array.
            pLocation->id(itAttr.key());
            pLocation->name(JsonCaster{serverObj.value(QStringLiteral("name"))});
            pLocation->country(JsonCaster{serverObj.value(QStringLiteral("country"))});
            pLocation->dns(JsonCaster{serverObj.value(QStringLiteral("dns"))});
            pLocation->portForward(JsonCaster{serverObj.value(QStringLiteral("port_forward"))});
            //The UDP and TCP connection addresses are objects in the source data,
            //they seem to just have a "best" attribute that contains the actual
            //connection address.
            pLocation->openvpnUDP(JsonCaster{serverObj.value(QStringLiteral("openvpn_udp")).toObject().value(QStringLiteral("best"))});
            pLocation->openvpnTCP(JsonCaster{serverObj.value(QStringLiteral("openvpn_tcp")).toObject().value(QStringLiteral("best"))});
            pLocation->ping(JsonCaster{serverObj.value(QStringLiteral("ping"))});
            pLocation->serial(serverObj.value(QStringLiteral("serial")).toString());

            // If safeAutoRegions is empty then there's likely a bug with the server data
            // and we work around it by just allowing 'connect auto' access to all regions -
            // the alternative is disallowing 'connect auto' to all regions, which is not really acceptable.
            if (!safeAutoRegions.empty())
            {
                // This server can be used with 'connect auto' if it's found in safeAutoRegions
                pLocation->isSafeForAutoConnect(safeAutoRegions.contains(pLocation->id()));
            }
            else
            {
                // This server can be used with 'connect auto'
                pLocation->isSafeForAutoConnect(true);
            }

            // This location is good, store it
            newLocations.insert(itAttr.key(), pLocation);
        }
        catch(const std::exception &ex)
        {
            qWarning() << "Can't load location" << itAttr.key()
                << "due to error" << ex.what();
        }
    }

    return newLocations;
}

ServerLocations updateShadowsocksLocations(const ServerLocations &existingLocations,
                                           const QJsonObject &shadowsocksObj)
{
    ServerLocations newLocations;

    // Copy each existing location.  We apply the change as an entire new
    // locations list, rather than mutating the locations, because the JSON
    // property change detection does not detect nested properties.
    newLocations.reserve(existingLocations.size());
    for(auto itExisting = existingLocations.begin(); itExisting != existingLocations.end(); ++itExisting)
    {
        if(*itExisting)
        {
            QSharedPointer<ServerLocation> pNewLocation{new ServerLocation{**itExisting}};
            // Wipe out the Shadowsocks server in case this region is gone.
            pNewLocation->shadowsocks({});
            newLocations.insert(itExisting.key(), pNewLocation);
        }
    }

    for(auto itAttr = shadowsocksObj.begin(); itAttr != shadowsocksObj.end(); ++itAttr)
    {
        // Do we have this region's data from the servers list?  We can't use
        // regions that don't appear in the servers list, ignore it if it's
        // not there.
        auto itLocation = newLocations.find(itAttr.key());
        if(itLocation == newLocations.end())
        {
            qWarning() << "Shadowsocks location" << itAttr.key()
                << "is not in the servers list, ignoring this location";
            continue;
        }
        Q_ASSERT(*itLocation);  // Consequence of above, nullptrs not copied

        // Create a ShadowsocksServer and apply it
        try
        {
            const auto &ssRgnObj = itAttr.value().toObject();
            QSharedPointer<ShadowsocksServer> pSsServer{new ShadowsocksServer{}};
            pSsServer->host(JsonCaster{ssRgnObj.value(QStringLiteral("host"))});
            pSsServer->port(JsonCaster{ssRgnObj.value(QStringLiteral("port"))});
            pSsServer->key(JsonCaster{ssRgnObj.value(QStringLiteral("key"))});
            pSsServer->cipher(JsonCaster{ssRgnObj.value(QStringLiteral("cipher"))});

            // Apply the new ShadowsocksServer; we can mutate this
            // ServerLocation because it's one that we just created above.
            (*itLocation)->shadowsocks(pSsServer);
        }
        catch(const std::exception &ex)
        {
            qWarning() << "Can't load Shadowsocks location" << itAttr.key()
                << "due to error" << ex.what();
        }
    }

    return newLocations;
}

// Compare two locations or countries to sort them.
// Sorts by latencies first, then country codes, then by IDs.
// The "tiebreaking" fields (country codes / IDs) are fixed to ensure that we
// sort regions the same way in all contexts.
bool compareEntries(const ServerLocation &first, const ServerLocation &second)
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

QVector<CountryLocations> buildGroupedLocations(const ServerLocations &locations)
{
    // Group the locations by country
    QHash<QString, QVector<QSharedPointer<ServerLocation>>> countryGroups;

    for(const auto &pLocation : locations)
    {
        Q_ASSERT(pLocation);
        countryGroups[pLocation->country().toLower()].push_back(pLocation);
    }

    // Sort each countries' locations by latency, then id
    for(auto &group : countryGroups)
    {
        std::sort(group.begin(), group.end(),
            [](const auto &pFirst, const auto &pSecond)
            {
                Q_ASSERT(pFirst);
                Q_ASSERT(pSecond);

                return compareEntries(*pFirst, *pSecond);
            });
    }

    // Create country groups from the sorted lists
    QVector<CountryLocations> countries;
    countries.reserve(countryGroups.size());
    for(const auto &group : countryGroups)
    {
        countries.push_back({});
        countries.last().locations(group);
    }

    // Sort the countries by their lowest latency
    std::sort(countries.begin(), countries.end(),
        [](const auto &first, const auto &second)
        {
            // Consequence of above; countries created with at least 1 location
            Q_ASSERT(!first.locations().isEmpty());
            Q_ASSERT(!second.locations().isEmpty());
            // Sort by the lowest latency for each country, then country code if
            // the latencies are the same
            const auto &pFirstNearest = first.locations().first();
            const auto &pSecondNearest = second.locations().first();

            Q_ASSERT(pFirstNearest);
            Q_ASSERT(pSecondNearest);
            return compareEntries(*pFirstNearest, *pSecondNearest);
        });

    return countries;
}

NearestLocations::NearestLocations(const ServerLocations &allLocations)
{
    _locations.reserve(allLocations.size());
    std::copy(allLocations.begin(), allLocations.end(),
              std::back_inserter(_locations));
    std::sort(_locations.begin(), _locations.end(),
                   [](const auto &pFirst, const auto &pSecond) {
                       Q_ASSERT(pFirst);
                       Q_ASSERT(pSecond);

                       return compareEntries(*pFirst, *pSecond);
                   });
}

QSharedPointer<ServerLocation> NearestLocations::getNearestSafeVpnLocation(bool portForward) const
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
                return location->portForward() && location->isSafeForAutoConnect();
            });
        if(result != _locations.end())
            return *result;
    }

    // otherwise just find the fastest 'safe' server
    auto result = std::find_if(_locations.begin(), _locations.end(),
        [](const auto &location) {return location->isSafeForAutoConnect();});
    if(result != _locations.end())
        return *result;

    // We fall-back to the fastest region since we could not find a region meeting the above constraints
    qWarning() << "Unable to find closest server location meeting constraints, falling back to fastest region";
    return _locations.front();
}

bool isDNSHandshake(const DaemonSettings::DNSSetting &setting)
{
    return setting == QStringLiteral("handshake");
}

const QString hnsdLocalAddress{QStringLiteral("127.80.73.65")};

QStringList getDNSServers(const DaemonSettings::DNSSetting& setting)
{
    if (setting == QStringLiteral("pia"))
        return { QStringLiteral("209.222.18.222"), QStringLiteral("209.222.18.218") };
    if (isDNSHandshake(setting))
        return { hnsdLocalAddress };
    QStringList servers;
    if (setting.get(servers))
        return servers;
    // Empty list (don't override DNS)
    return {};
}

#endif
