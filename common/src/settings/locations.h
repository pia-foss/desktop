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

#ifndef SETTINGS_LOCATIONS_H
#define SETTINGS_LOCATIONS_H

#include "../common.h"
#include "../json.h"
#include <kapps_regions/src/region.h>
#include <kapps_regions/src/regiondisplay.h>
#include <set>
#include <unordered_map>

// These are the services advertised by the regions list that are used by Desktop.
enum class Service
{
    OpenVpnTcp,
    OpenVpnUdp,
    WireGuard,
    Shadowsocks,
    // The "meta" service provides access to servers lists and the web API in
    // the modern infrastructure.
    Meta,
};

// The available connection ports are listed and attempted in descending order.
// This is subtle, but it generally places the ports that feel more natural at
// the top (defaults, 8080, TCP 443), and unusual ports that are repurposed for
// the VPN (DNS/POP/HTTP) at the bottom.
using DescendingPortSet = std::set<quint16, std::greater<quint16>>;

// Server describes a single server in a region, which may provide any
// combination of services.
//
// This is used to build the state representation sent to the client -
// serializing to JSON only writes the information needed by clients.
// (Currently, just the IP and common name.)
class COMMON_EXPORT Server
{
public:
    Server(std::shared_ptr<const kapps::regions::Server> pImpl);

public:
    bool operator==(const Server &other) const
    {
        return ip() == other.ip() &&
            commonName() == other.commonName() &&
            openVpnTcpPorts() == other.openVpnTcpPorts() &&
            openVpnUdpPorts() == other.openVpnUdpPorts() &&
            wireGuardPorts() == other.wireGuardPorts() &&
            shadowsocksPorts() == other.shadowsocksPorts() &&
            metaPorts() == other.metaPorts() &&
            shadowsocksKey() == other.shadowsocksKey() &&
            shadowsocksCipher() == other.shadowsocksCipher() &&
            openVpnTcpNcp() == other.openVpnTcpNcp() &&
            openVpnUdpNcp() == other.openVpnUdpNcp();
    }
    bool operator!=(const Server &other) const {return !(*this == other);}

public:
    // Get the underlying kapps::regions::Server - eventually these wrappers
    // can probably be removed, and we'll just use kapps::regions::Server.
    const kapps::regions::Server &impl() const
    {
        Q_ASSERT(_pImpl);   // Class invariant
        return *_pImpl;
    }

    // The server's IP address (used for all services)
    QString ip() const {return qs::toQString(_pImpl->address().toString());}
    // The server certificate CN to expect
    QString commonName() const {return qs::toQString(_pImpl->commonName());}

    // These fields identify the available ports on this server for each
    // possible service.  If a server doesn't have a particular service, that
    // list is empty.  The first port is the "default" port for this server.
    //
    // There are also other services advertised that are not used by Desktop.
    kapps::regions::Ports openVpnTcpPorts() const {return _pImpl->openVpnTcpPorts();}
    kapps::regions::Ports openVpnUdpPorts() const {return _pImpl->openVpnUdpPorts();}
    kapps::regions::Ports wireGuardPorts() const {return _pImpl->wireGuardPorts();}
    kapps::regions::Ports shadowsocksPorts() const {return _pImpl->shadowsocksPorts();}
    kapps::regions::Ports metaPorts() const {return _pImpl->metaPorts();}

    // Service-specific additional fields

    // For servers with the Shadowsocks service, the key and cipher used to
    // connect
    QString shadowsocksKey() const {return qs::toQString(_pImpl->shadowsocksKey());}
    QString shadowsocksCipher() const {return qs::toQString(_pImpl->shadowsocksCipher());}

    // For servers with the OpenVPN service, whether we use NCP cipher
    // negotiation (the default), or pia-signal-settings negotiation.
    // We're transitioning away from pia-signal-settings - the client currently
    // supports both, but eventually we'll start deploying servers without the
    // patch, at which point the client will use NCP cipher negotiation.
    bool openVpnTcpNcp() const {return _pImpl->openVpnTcpNcp();}
    bool openVpnUdpNcp() const {return _pImpl->openVpnUdpNcp();}

public:
    // Check whether this server has a given service.
    bool hasService(Service service) const;
    // Check whether this server has any VPN service (any OpenVPN or WireGuard)
    bool hasVpnService() const;
    // Check whether this server offers a specific port for the given service.
    bool hasPort(Service service, quint16 port) const;
    // Get/set the given service - returns or sets one of the vectors above
    kapps::regions::Ports servicePorts(Service service) const;
    // Default port for a service on this server (first port from list), or 0
    // if that service isn't provided
    quint16 defaultServicePort(Service service) const;
    // Random port for a service available from this server
    quint16 randomServicePort(Service service) const;

private:
    std::shared_ptr<const kapps::regions::Server> _pImpl;
};

// Location describes a single location, which can contain any number of servers
// and services.  Some services may not be present in some regions at all.
//
// PIA Desktop currently supports both the "current" and "new" servers lists.
// These are represented with different models.  The current servers list is
// adapted to the format of the new region list.
//
// Regions from each infrastructure could be matched by ID if needed, but keep
// in mind that region metadata like the name/country might vary between the two
// infrastructures.
class COMMON_EXPORT Location
{
public:
    Location(std::shared_ptr<const kapps::regions::Region> pImpl,
             nullable_t<double> latency);

    bool operator==(const Location &other) const
    {
        return id() == other.id() &&
            portForward() == other.portForward() &&
            geoLocated() == other.geoLocated() &&
            autoSafe() == other.autoSafe() &&
            latency() == other.latency() &&
            servers() == other.servers() &&
            dedicatedIp() == other.dedicatedIp() &&
            offline() == other.offline() &&
            hasShadowsocks() == other.hasShadowsocks();
    }
    bool operator!=(const Location &other) const {return !(*this == other);}

public:
    // The region's ID.  This is the immutable identifier for this region, which
    // is used to identify location choices, favorites, etc.  Avoid displaying
    // this in the UI (except possibly as a last resort).
    QString id() const {return qs::toQString(_pImpl->id());}
    // Whether this region has port forwarding
    bool portForward() const {return _pImpl->portForward();}
    // Whether this location is provided by geolocation only.
    bool geoLocated() const {return _pImpl->geoLocated();}
    // Whether this region should be considered for automatic selection.  Ops
    // can turn this off to reduce load on a particular region while leaving it
    // available for manual selection, etc.
    bool autoSafe() const {return _pImpl->autoSafe();}

    // Latency is recorded for the whole region.  Eventually we may start
    // measuring individual servers in the nearest regions.
    nullable_t<double> latency() const {return _latency;}

    // The available servers in this region.  These are all grouped together,
    // not separated by type, because servers could contain any combination of
    // services, and the combinations of services could change at any time.
    //
    // For example, today there are "VPN" servers that provide OpenVPN UDP,
    // OpenVPN TCP, and WireGuard, but it's possible that in the future there
    // could be servers that have OpenVPN UDP and WireGuard without OpenVPN TCP,
    // in order to use TCP 443 for something else on the same server.
    //
    // When selecting a server for a needed service, the client should obviously
    // look for servers that offer that service.
    //
    // In some cases, it may be preferred to use the same server for multiple
    // services (such as OpenVPN via Shadowsocks in the same region, which would
    // have the least latency when using the same server for both).  The client
    // must be prepared for the possibility that there might be no servers with
    // both services, though.
    kapps::core::ArraySlice<const Server> servers() const {return _servers;}

    // For a dedicated IP region, the dedicated IP address is provided.  This is
    // empty for normal regions.
    //
    // This is useful in UI to display the proper information about the
    // dedicated IP server.  The IP/CN will also appear in one or more Server
    // objects, but other types of Servers can appear too (like meta servers
    // taken from a corresponding region).
    //
    // Dedicated IP regions are stored alongside regular regions in
    // DaemonState::availableLocations (or in general in LocationsById maps),
    // but are not included in DaemonState::groupedLocations (as that is used
    // for UI display, and dedicated IP regions are displayed differently; they
    // are provided in sort order in in DaemonState::dedicatedIpLocations).
    QString dedicatedIp() const;

    // Some regions might be marked offline in the servers list
    // This indicates the region should not be used and should be designated
    // as unavailable in the UI
    bool offline() const {return _pImpl->offline();}

    // Whether this region offers Shadowsocks (this is the only service the
    // UI cares about distinguishing)
    bool hasShadowsocks() const {return _pImpl->hasService(kapps::regions::Service::Shadowsocks);}

private:
    // Count the servers that satisfy a predicate
    template<class PredicateFuncT>
    std::size_t countServersFor(const PredicateFuncT &predicate) const;
    // Get a random server that satisfies a predicate
    template<class PredicateFuncT>
    const Server *randomServerFor(const PredicateFuncT &predicate) const;

    template<class PredicateFuncT>
    const Server *serverWithIndexFor(std::size_t desiredIndex, const PredicateFuncT &predicate) const;

public:
    // Check if a given service is available in this location (whether any
    // server has the service)
    bool hasService(Service service) const;

    // Check whether this is a dedicated IP location.
    bool isDedicatedIp() const {return _pImpl->isDedicatedIp();}

    // Get any random server in the location suitable for measuring latency with
    // ICMP pings (for the modern infrastructure).
    // This picks any random server with any VPN service (servers with only
    // Shadowsocks or UdpLatency are ignored).
    // Only servers with a VPN service are used because the Shadowsocks run on
    // separate infrastructure and may not be representative of the latency to
    // the VPN servers, which is much more important.
    const Server *randomIcmpLatencyServer() const;

    // Get a random server that has a given service (nullptr if there are none)
    const Server *randomServerForService(Service service) const;

    // Get a random server for a given service and port (nullptr if there are
    // none)
    const Server *randomServerForPort(Service service, quint16 port) const;

    // Get a random server for the given service, and try to get the port
    // specified (if any).  If the port is nonzero, and there is at least one
    // server with that port, a server with that port is selected.  Otherwise,
    // any server that provides the service is selected.
    const Server *randomServer(Service service, quint16 tryPort) const;

    // Get all available ports for a service in this region.  This is an ordered
    // set so the attempt order in TransportSelector is consistent.
    DescendingPortSet allPortsForService(Service service) const;
    // Get all available ports for a service - append them to an existing set
    // rather than returning a new set (used by Daemon to find the global set of
    // all ports)
    void allPortsForService(Service service, DescendingPortSet &ports) const;

    const Server *serverWithIndex(std::size_t index, Service service, quint16 tryPort) const;
    const Server *serverWithIndexForPort(std::size_t index, Service service, quint16 port) const;
    const Server *serverWithIndexForService(std::size_t index, Service service) const;

    // Count the servers that support a given service; used to implement
    // randomServerForService() and allServersForService()
    std::size_t countServersForService(Service service) const;
    std::size_t countServersForPort(Service service, quint16 port) const;

private:
    std::shared_ptr<const kapps::regions::Region> _pImpl;
    nullable_t<double> _latency;
    std::vector<Server> _servers;
};

// Compare QSharedPointer<Location>s by value; used by ServiceLocations
// and ConnectionInfo
inline bool compareLocationsValue(const QSharedPointer<const Location> &pFirst,
                                  const QSharedPointer<const Location> &pSecond)
{
    // If one is nullptr, they're only the same if they're both nullptr
    if(!pFirst || !pSecond)
        return pFirst == pSecond;
    return *pFirst == *pSecond;
}

using LocationsById = std::unordered_map<std::string, QSharedPointer<const Location>>;
using LatencyMap = std::unordered_map<QString, double>;

// Locations for a given country, sorted by latency (ties broken by id).
class COMMON_EXPORT CountryLocations
{
public:
    CountryLocations() = default;
    CountryLocations(std::string code,
        std::vector<QSharedPointer<const Location>> locations)
        : _code{std::move(code)}, _locations{std::move(locations)}
    {}

    bool operator==(const CountryLocations &other) const
    {
        return code() == other.code() && locations() == other.locations();
    }
    bool operator!=(const CountryLocations &other) const {return !(*this == other);}

public:
    kapps::core::StringSlice code() const {return _code;}
    kapps::core::ArraySlice<const QSharedPointer<const Location>> locations() const {return _locations;}

private:
    std::string _code;
    std::vector<QSharedPointer<const Location>> _locations;
};


// Class listing the various location choices that the daemon makes for a given
// "service" (currently either the VPN or Shadowsocks).
//
// Daemon does not care about most of the "intermediate steps", but they're
// centralized here because the client's display needs to match up with the
// daemon's internal logic.
//
// These are in DaemonState because they are not persisted, they're rebuilt from
// DaemonData/DaemonSettings.
//
// chosenLocation, bestLocation, and nextLocation are (when valid) locations
// from the current locations list.  (The QSharedPointers point to the same
// object from that list.)
class COMMON_EXPORT ServiceLocations
{
public:
    ServiceLocations() = default;
    ServiceLocations(QSharedPointer<const Location> pChosenLocation,
        QSharedPointer<const Location> pBestLocation,
        QSharedPointer<const Location> pNextLocation);

public:
    bool operator==(const ServiceLocations &other) const
    {
        return compareLocationsValue(chosenLocation(), other.chosenLocation()) &&
            compareLocationsValue(bestLocation(), other.bestLocation()) &&
            compareLocationsValue(nextLocation(), other.nextLocation());
    }
    bool operator!=(const ServiceLocations &other) const {return !(*this == other);}

public:
    // The daemon's interpretation of the selected location.  If
    // DaemonSettings::location is a valid non-auto location, this is that
    // location's object.  Otherwise, it is undefined, which indicates 'auto'.
    //
    // (The daemon interprets an invalid location as 'auto'.)
    const QSharedPointer<const Location> &chosenLocation() const {return _pChosenLocation;}
    // The best location (the location we would choose for 'auto', regardless of
    // whether 'auto' is actually selected).  This is undefined if and only if
    // no locations are currently known.
    //
    // For the VPN service, bestLocation is the lowest-latency location, or the
    // lowest-latency location that supports port forwarding if it is enabled.
    //
    // For the Shadowsocks service, bestLocation is the next *VPN* location if
    // it has Shadowsocks, otherwise the lowest-latency location with
    // Shadowsocks.  (Note that the best SS location therefore depends on
    // the chosen VPN location.)
    const QSharedPointer<const Location> &bestLocation() const {return _pBestLocation;}
    // The next location we would connect to if the user chooses to connect (or
    // reconnect) right now.
    //
    // Like 'chosenLocation', undefined if and only if no locations are known.
    const QSharedPointer<const Location> &nextLocation() const {return _pNextLocation;}

private:
    QSharedPointer<const Location> _pChosenLocation;
    QSharedPointer<const Location> _pBestLocation;
    QSharedPointer<const Location> _pNextLocation;
};

#endif
