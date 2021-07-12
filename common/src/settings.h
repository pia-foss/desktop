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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "common.h"
#include "json.h"
#include "version.h"
#include <QVector>
#include <QHostAddress>
#include <set>
#include <unordered_set>
#include <deque>

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
class COMMON_EXPORT Server : public NativeJsonObject
{
    Q_OBJECT
public:
    Server() {}
    Server(const Server &other) {*this = other;}
    Server &operator=(const Server &other)
    {
        ip(other.ip());
        commonName(other.commonName());
        openvpnTcpPorts(other.openvpnTcpPorts());
        openvpnUdpPorts(other.openvpnUdpPorts());
        wireguardPorts(other.wireguardPorts());
        shadowsocksPorts(other.shadowsocksPorts());
        metaPorts(other.metaPorts());
        shadowsocksKey(other.shadowsocksKey());
        shadowsocksCipher(other.shadowsocksCipher());
        openvpnNcpSupport(other.openvpnNcpSupport());
        return *this;
    }

    bool operator==(const Server &other) const
    {
        return ip() == other.ip() && commonName() == other.commonName() &&
            openvpnTcpPorts() == other.openvpnTcpPorts() &&
            openvpnUdpPorts() == other.openvpnUdpPorts() &&
            wireguardPorts() == other.wireguardPorts() &&
            shadowsocksPorts() == other.shadowsocksPorts() &&
            metaPorts() == other.metaPorts() &&
            shadowsocksKey() == other.shadowsocksKey() &&
            shadowsocksCipher() == other.shadowsocksCipher() &&
            openvpnNcpSupport() == other.openvpnNcpSupport();
    }
    bool operator!=(const Server &other) const {return !(*this == other);}

public:
    // The server's IP address (used for all services)
    JsonField(QString, ip, {})
    // The server certificate CN to expect
    JsonField(QString, commonName, {})

    // These fields identify the available ports on this server for each
    // possible service.  If a server doesn't have a particular service, that
    // list is empty.  The first port is the "default" port for this server.
    //
    // There are also other services advertised that are not used by Desktop.
    JsonField(std::vector<quint16>, openvpnTcpPorts, {})
    JsonField(std::vector<quint16>, openvpnUdpPorts, {})
    JsonField(std::vector<quint16>, wireguardPorts, {})
    JsonField(std::vector<quint16>, shadowsocksPorts, {})
    JsonField(std::vector<quint16>, metaPorts, {})

    // Service-specific additional fields

    // For servers with the Shadowsocks service, the key and cipher used to
    // connect
    JsonField(QString, shadowsocksKey, {})
    JsonField(QString, shadowsocksCipher, {})

    // For servers with the OpenVPN service, whether we use NCP cipher
    // negotiation (the default), or pia-signal-settings negotiation.
    // We're transitioning away from pia-signal-settings - the client currently
    // supports both, but eventually we'll start deploying servers without the
    // patch, at which point the client will use NCP cipher negotiation.
    JsonField(bool, openvpnNcpSupport, true)

public:
    // Check whether this server has any service known to Desktop other than
    // Latency - used to ignore servers with no known services.
    bool hasNonLatencyService() const;
    // Check whether this server has a given service.
    bool hasService(Service service) const;
    // Check whether this server has any VPN service (any OpenVPN or WireGuard)
    bool hasVpnService() const;
    // Check whether this server offers a specific port for the given service.
    bool hasPort(Service service, quint16 port) const;
    // Get/set the given service - returns or sets one of the vectors above
    const std::vector<quint16> &servicePorts(Service service) const;
    void servicePorts(Service service, std::vector<quint16> ports);
    // Default port for a service on this server (first port from list), or 0
    // if that service isn't provided
    quint16 defaultServicePort(Service service) const;
    // Random port for a service available from this server
    quint16 randomServicePort(Service service) const;
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
class COMMON_EXPORT Location : public NativeJsonObject
{
    Q_OBJECT
public:
    Location() {}
    Location(const Location &other) {*this = other;}
    Location &operator=(const Location &other)
    {
        id(other.id());
        name(other.name());
        country(other.country());
        portForward(other.portForward());
        geoOnly(other.geoOnly());
        autoSafe(other.autoSafe());
        latency(other.latency());
        servers(other.servers());
        dedicatedIpExpire(other.dedicatedIpExpire());
        dedicatedIp(other.dedicatedIp());
        dedicatedIpCorrespondingRegion(other.dedicatedIpCorrespondingRegion());
        offline(other.offline());
        return *this;
    }

    bool operator==(const Location &other) const
    {
        return id() == other.id() && name() == other.name() &&
            country() == other.country() && portForward() == other.portForward() &&
            geoOnly() == other.geoOnly() && autoSafe() == other.autoSafe() &&
            latency() == other.latency() && servers() == other.servers() &&
            dedicatedIpExpire() == other.dedicatedIpExpire() &&
            dedicatedIp() == other.dedicatedIp() &&
            dedicatedIpCorrespondingRegion() == other.dedicatedIpCorrespondingRegion() &&
            offline() == other.offline();
    }
    bool operator!=(const Location &other) const {return !(*this == other);}

public:
    // The region's ID.  This is the immutable identifier for this region, which
    // is used to identify location choices, favorites, etc.  Avoid displaying
    // this in the UI (except possibly as a last resort).
    JsonField(QString, id, {})
    // Region name.  Desktop ships region names and translations (see
    // DaemonSettings.qml), but we still check the advertised name to make sure our
    // translation is still accurate.
    JsonField(QString, name, {})
    // Country code for this region - used to group regions and to display
    // country flags/names.
    JsonField(QString, country, {})
    // Whether this region has port forwarding
    JsonField(bool, portForward, false)
    // Whether this location is provided by geolocation only.
    JsonField(bool, geoOnly, false)
    // Whether this region should be considered for automatic selection.  Ops
    // can turn this off to reduce load on a particular region while leaving it
    // available for manual selection, etc.
    JsonField(bool, autoSafe, true)

    // Latency is recorded for the whole region.  Eventually we may start
    // measuring individual servers in the nearest regions.
    JsonField(Optional<double>, latency, {})

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
    JsonField(std::vector<Server>, servers, {})

    // For a dedicated IP region, the expiration time and dedicated IP address
    // are both provided.  These are 0/{} for normal regions.
    //
    // This is used when connecting to handle authentication properly, which is
    // different for a dedicated IP region.  Detect dedicated IP regions with
    // isDedicatedIp() (which tests dedicatedIpExpire(); we don't validate
    // the IP address from the server so we can't guarantee that dedicatedIp()
    // is non-empty).
    //
    // Dedicated IP regions are stored alongside regular regions in
    // DaemonState::availableLocations (or in general in LocationsById maps),
    // but are not included in DaemonState::groupedLocations (as that is used
    // for UI display, and dedicated IP regions are displayed differently; they
    // are provided in sort order in in DaemonState::dedicatedIpLocations).
    JsonField(quint64, dedicatedIpExpire, 0)
    JsonField(QString, dedicatedIp, {})
    // The "corresponding region" ID is just used to get geo coords for the
    // location maps.  The geo coords currently aren't folded into the location
    // objects, because they are fetched separately from the region metadata
    // API.
    JsonField(QString, dedicatedIpCorrespondingRegion, {})

    // Some regions might be marked offline in the servers list
    // This indicates the region should not be used and should be designated
    // as unavailable in the UI
    JsonField(bool, offline, false)

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
    bool isDedicatedIp() const {return dedicatedIpExpire() > 0;}

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

    // Get all servers for a given service
    std::vector<Server> allServersForService(Service service) const;

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
};

using LocationsById = std::unordered_map<QString, QSharedPointer<Location>>;
using LatencyMap = std::unordered_map<QString, double>;

// Locations for a given country, sorted by latency (ties broken by id).
class COMMON_EXPORT CountryLocations : public NativeJsonObject
{
    Q_OBJECT
public:
    CountryLocations() {}
    CountryLocations(const CountryLocations &other) {*this = other;}
    CountryLocations &operator=(const CountryLocations &other)
    {
        locations(other.locations());
        return *this;
    }

    bool operator==(const CountryLocations &other) const
    {
        return locations() == other.locations();
    }
    bool operator!=(const CountryLocations &other) const {return !(*this == other);}

    JsonField(std::vector<QSharedPointer<Location>>, locations, {})
};

// Bandwidth measurements for one measurement interval, in bytes
class COMMON_EXPORT IntervalBandwidth : public NativeJsonObject
{
    Q_OBJECT
public:
    IntervalBandwidth() {}
    IntervalBandwidth(quint64 receivedVal, quint64 sentVal)
    {
        received(receivedVal);
        sent(sentVal);
    }
    IntervalBandwidth(const IntervalBandwidth &other) {*this = other;}
    IntervalBandwidth &operator=(const IntervalBandwidth &other)
    {
        received(other.received());
        sent(other.sent());
        return *this;
    }
    bool operator==(const IntervalBandwidth &other) const
    {
        return received() == other.received() && sent() == other.sent();
    }
    bool operator!=(const IntervalBandwidth &other) const
    {
        return !(*this == other);
    }

    JsonField(quint64, received, {})
    JsonField(quint64, sent, {})
};

class COMMON_EXPORT AppMessage : public NativeJsonObject
{
    Q_OBJECT
public:
    using TextTranslations = std::unordered_map<QString, QString>;
public:
    AppMessage() {}

    AppMessage(const AppMessage &other) {*this = other;}
    AppMessage &operator=(const AppMessage &other)
    {
        id(other.id());
        hasLink(other.hasLink());
        messageTranslations(other.messageTranslations());
        linkTranslations(other.linkTranslations());
        settingsAction(other.settingsAction());
        viewAction(other.viewAction());
        uriAction(other.uriAction());
        return *this;
    }

    bool operator==(const AppMessage &other) const
    {
        return id() == other.id() && hasLink() == other.hasLink() &&
        messageTranslations() == other.messageTranslations() &&
        linkTranslations() == other.linkTranslations() &&
        settingsAction() == other.settingsAction() &&
        viewAction() == other.viewAction() &&
        uriAction() == other.uriAction();
    }

    bool operator!=(const AppMessage &other) const
    {
        return !(*this == other);
    }

public:
    JsonField(quint64, id, 0)
    JsonField(bool, hasLink, false)
    JsonField(TextTranslations, messageTranslations, {})
    JsonField(TextTranslations, linkTranslations, {})
    JsonField(QJsonObject, settingsAction, {})
    JsonField(QString, viewAction, {})
    JsonField(QString, uriAction, {})
};

// Transport settings that might vary due to automatic failover.
class COMMON_EXPORT Transport : public NativeJsonObject
{
    Q_OBJECT

public:
    Transport() {}
    Transport(const QString &protocolVal, uint portVal)
    {
        protocol(protocolVal);
        port(portVal);
    }
    Transport(const Transport &other) {*this = other;}
    Transport &operator=(const Transport &other)
    {
        protocol(other.protocol());
        port(other.port());
        return *this;
    }
    bool operator==(const Transport &other) const
    {
        return protocol() == other.protocol() && port() == other.port();
    }
    bool operator!=(const Transport &other) const
    {
        return !(*this == other);
    }

    JsonField(QString, protocol, QStringLiteral("udp"), { "udp", "tcp" })
    JsonField(uint, port, 0)

public:
    // If this transport had port 0 ("use default"), and the actual protocol
    // selected is the same as this transport's protocol, apply the default
    // port from the selected server.  This is used for the "preferred"
    // transport so the client can tell whether a preferred "default" transport
    // is the same as the actual transport.
    void resolveDefaultPort(const QString &selectedProtocol, const Server *pSelectedServer);

    // Based on the transport's protocol and port, select a server from the
    // specified location, and determine the actual port to use to connect.  If
    // no connection is possible, the port is set to 0.
    //
    // - A server is selected from the location given based on the protocol.  If
    //   the transport has a specific port, it will try to get a server with
    //   that port.
    // - If the port is currently 0, the default port from the selected server
    //   is used
    // - If the port is nonzero but that port is not available on this server,
    //   the default port from the selected server is used
    //
    // Returns the appropriate server for this transport's protocol (regardless
    // of whether a port is found).
    const Server *selectServerPort(const Location &location);
    const Server *selectServerPortWithIndex(const Location &location, size_t index);
    std::size_t countServersForLocation(const Location &location) const;
};

// Definition of a custom SOCKS proxy (see DaemonSettings::customProxy)
class COMMON_EXPORT CustomProxy : public NativeJsonObject
{
    Q_OBJECT

public:
    CustomProxy() {}
    CustomProxy(const CustomProxy &other) {*this = other;}
    CustomProxy &operator=(const CustomProxy &other)
    {
        host(other.host());
        port(other.port());
        username(other.username());
        password(other.password());
        return *this;
    }

    bool operator==(const CustomProxy &other) const
    {
        return host() == other.host() && port() == other.port() &&
            username() == other.username() && password() == other.password();
    }

    bool operator!=(const CustomProxy &other) const
    {
        return !(*this == other);
    }

    // Host - domain name or IPv4 address.
    JsonField(QString, host, {})

    // Remote port - 0 just indicates the default (1080)
    JsonField(uint, port, 0)

    // Username/password - both optional
    JsonField(QString, username, {})
    JsonField(QString, password, {})
};

// Information for a Dedicated IP stored in DaemonAccount.  The daemon uses
// this to populate "dedicated IP regions" in the regions list, and to refresh
// the dedicated IP information periodically.
class COMMON_EXPORT AccountDedicatedIp : public NativeJsonObject
{
    Q_OBJECT
public:
    AccountDedicatedIp() {}
    AccountDedicatedIp(const AccountDedicatedIp &other) {*this = other;}
    AccountDedicatedIp &operator=(const AccountDedicatedIp &other)
    {
        dipToken(other.dipToken());
        expire(other.expire());
        id(other.id());
        regionId(other.regionId());
        serviceGroups(other.serviceGroups());
        ip(other.ip());
        cn(other.cn());
        lastIpChange(other.lastIpChange());
        return *this;
    }

    bool operator==(const AccountDedicatedIp &other) const
    {
        return dipToken() == other.dipToken() &&
            expire() == other.expire() &&
            id() == other.id() &&
            regionId() == other.regionId() &&
            serviceGroups() == other.serviceGroups() &&
            ip() == other.ip() &&
            cn() == other.cn() &&
            lastIpChange() == other.lastIpChange();
    }
    bool operator!=(const AccountDedicatedIp &other) const {return !(*this == other);}

    // This is the DIP token.  The token is functionally a password - it must
    // not be exposed in the region information provided in DaemonState.
    JsonField(QString, dipToken, {})

    // This is the expiration timestamp - UTC Unix time in milliseconds.
    // Note that the API returns this in seconds; it's stored in milliseconds
    // to match all other timestamps in Desktop.
    JsonField(quint64, expire, 0)

    // The "ID" created by the client to represent this region in the region
    // list - of the form "dip-###", where ### is a random number.
    //
    // A random identifier is used rather than the token or IP address to meet
    // the following requirements:
    // * DIP tokens can't be exposed in the region information in DaemonState
    //   (they're functionally passwords)
    // * The region ID must remain the same even in the rare event that the
    //   dedicated IP changes (we can't use the IP address itself)
    // * The region ID should not be likely to duplicate a prior region ID that
    //   was added and expired/removed (could cause minor oddities like a
    //   favorite reappearing, etc.)
    //
    // This is not the 'id' field from the DIP API, that's regionId.
    JsonField(QString, id, {})

    // The ID of the corresponding PIA region where this server is.  We get the
    // country code, region name, geo flag, and PF flag from the corresponding
    // PIA region.  We also pull auxiliary service servers from this region,
    // like meta and Shadowsocks.
    JsonField(QString, regionId, {})

    // These are the server groups (from the server list) provided by the DIP
    // server.  Note that like the server list, the client does not attribute
    // any significance to the group names themselves, which can be freely
    // changed by Ops.  This is just used to find the service and port
    // information from the servers list.
    JsonField(std::vector<QString>, serviceGroups, {})

    // The dedicated IP itself.  This is both the VPN endpoint and the VPN IP.
    // It provides all services identified by serviceGroups.
    JsonField(QString, ip, {})

    // The common name for certificates corresponding to this server.
    JsonField(QString, cn, {})

    // If the daemon observes a change in the dedicated IP's IP address, this
    // field is set to the most recent change timestamp.  This triggers a
    // notification in the client.  It's important to keep track of this
    // individually for each dedicated IP to ensure that the notification is
    // cleared if the changed dedicated IP is removed.
    JsonField(quint64, lastIpChange, 0)
};

// Event properties for ServiceQualityEvents - in an object property as expected
// by the API.  This would make more sense as a nested class, but moc doesn't
// support nested classes.
class EventProperties : public NativeJsonObject
{
    Q_OBJECT
public:
    EventProperties() = default;
    EventProperties(const EventProperties &other) { *this = other; }
    EventProperties &operator=(const EventProperties &other)
    {
        user_agent(other.user_agent());
        platform(other.platform());
        version(other.version());
        vpn_protocol(other.vpn_protocol());
        connection_source(other.connection_source());
        return *this;
    }
    bool operator==(const EventProperties &other) const
    {
        return user_agent() == other.user_agent() &&
            platform() == other.platform() &&
            version() == other.version() &&
            vpn_protocol() == other.vpn_protocol() &&
            connection_source() == other.connection_source();
    }
    bool operator!=(const EventProperties &other) const {return !(*this == other);}

public:
    // User agent; client information
    JsonField(QString, user_agent, QStringLiteral(PIA_USER_AGENT))
    JsonField(QString, platform, QStringLiteral(PIA_PLATFORM_NAME))
    JsonField(QString, version, QStringLiteral(PIA_VERSION))
    JsonField(bool, prerelease, PIA_VERSION_IS_PRERELEASE)
    // The VPN protocol being used
    JsonField(QString, vpn_protocol, {}, {"OpenVPN", "WireGuard"})
    // The source of the connection:
    // - "Manual" - The user directly initiated this connection (e.g. by
    //   clicking the Connect button).  CLI "connect" is also interpreted
    //   as manual.
    // - "Automatic" - The connection was initiated automatically (e.g. by
    //   ending Snooze or an automation rule.)
    JsonField(QString, connection_source, {}, {"Manual", "Automatic"})
};

// If the user has opted in to providing service quality information back to us
// (see DaemonSettings::sendServiceQualityEvents), service quality events are
// stored using this format.  We usually batch up to 20 events before sending
// them, and then hold onto events that were sent for up to 24 hours so they
// can be shown in the UI.
//
// Everything sent with an event is stored here, even though the
// version/platform/user agent/etc. rarely change, to ensure that we don't
// inadvertently send old events with a newer version, etc., after upgrading.
// This also conveniently means that we can make this match the format expected
// by the API.  (That's why the properties here use snake_case instead of our
// usual camelCase.)
class COMMON_EXPORT ServiceQualityEvent : public NativeJsonObject
{
    Q_OBJECT

public:
    ServiceQualityEvent() = default;
    ServiceQualityEvent(const ServiceQualityEvent &other) { *this = other; }
    ServiceQualityEvent &operator=(const ServiceQualityEvent &other)
    {
        aggregated_id(other.aggregated_id());
        event_unique_id(other.event_unique_id());
        event_name(other.event_name());
        event_time(other.event_time());
        event_token(other.event_token());
        event_properties(other.event_properties());
        return *this;
    }
    bool operator==(const ServiceQualityEvent &other) const
    {
        return aggregated_id() == other.aggregated_id() &&
            event_unique_id() == other.event_unique_id() &&
            event_name() == other.event_name() &&
            event_time() == other.event_time() &&
            event_token() == other.event_token() &&
            event_properties() == other.event_properties();
    }
    bool operator!=(const ServiceQualityEvent &other) const {return !(*this == other);}

public:
    // Aggregation ID at the time this event was generated - needed so we can
    // differentiate a large number of errors from a few users vs. a few errors
    // from a lot of users.  See DaemonState::qualityAggId, which is rotated
    // every 24 hours for privacy.
    JsonField(QString, aggregated_id, {})
    // Unique ID for this event.  This ensures a given event is only stored once
    // even if an API error causes us to retry a request after the API actually
    // received the payload.
    JsonField(QString, event_unique_id, {})
    // The event that occurred:
    // - Attempt - the VPN connection has been enabled.  Note that this is
    //   different from an "attempt" used elsewhere in PIA Desktop; this does
    //   _not_ count each connection attempt before a connection is established,
    //   and it does not count reconnects.
    // - Established - We successfully established the VPN connection after
    //   previously generating an "attempt" (i.e. after the connection was
    //   enabled, does not count reconnects)
    // - Canceled - The VPN connection was disabled before having established
    //   a connection (i.e. the user canceled).
    //
    // Note that connections in PIA Desktop rarely ever fail completely, in
    // most circumstances we just keep trying forever.  Mostly this occurs for
    // an authentication failure as there is no point retrying that error.  If
    // these occur in any significant number, they're apparent as the residual
    // (attempt - (established + canceled)).
    JsonField(QString, event_name, {}, {"VPN_CONNECTION_ATTEMPT",
        "VPN_CONNECTION_ESTABLISHED", "VPN_CONNECTION_CANCELED"})
    // Event timestamp - UTC Unix time in _seconds_ (not milliseconds like we
    // use elsewhere).
    // Note that PIA only stores timestamps with hourly granularity for privacy.
    JsonField(qint64, event_time, 0)
    // "Event token" - this identifies the product being used (here, PIA Desktop)
    // and the environment (staging or production, which for us corresponds to
    // prerelease builds vs. general releases).  This is shown as "Product ID"
    // in the UI.
#if PIA_VERSION_IS_PRERELEASE
    #if defined(Q_OS_WIN)
        #define PIA_PRODUCT_EVENT_TOKEN "b3fc3640ac22aedba0983c2cb38d60a3"  // Windows staging
    #elif defined(Q_OS_MACOS)
        #define PIA_PRODUCT_EVENT_TOKEN "3f670cc9ed90701ae79097eff72bc103"  // macOS staging
    #elif defined(Q_OS_LINUX)
        #define PIA_PRODUCT_EVENT_TOKEN "2b6487b0b55f39fd72214f47a8075179"  // Linux staging
    #endif
#else
    #if defined(Q_OS_WIN)
        #define PIA_PRODUCT_EVENT_TOKEN "c4f380b3d4b3a8c25402324f960911e2"  // Windows production
    #elif defined(Q_OS_MACOS)
        #define PIA_PRODUCT_EVENT_TOKEN "f3ccd0c159f102d2c5fd3383cb3b354d"  // macOS production
    #elif defined(Q_OS_LINUX)
        #define PIA_PRODUCT_EVENT_TOKEN "fd40e5dc1611d76649607f86334f27ea"  // Linux production
    #endif
#endif
    JsonField(QString, event_token, QStringLiteral(PIA_PRODUCT_EVENT_TOKEN))

    JsonObjectField(EventProperties, event_properties, {})
};

// Class encapsulating 'data' properties of the daemon; these are cached
// and persist between daemon instances.
//
// Most of these are fetched from servers at runtime in some way (regions lists,
// update metadata, latency measurements); they're cached for use in case we
// aren't able to fetch the data at some point in the future, and so we (likely)
// have some data to work with at startup.
//
// Cached data are stored in the original format received from the server, not
// an internalized format.  The caches may be reused even after a daemon update
// - internalized formats are likely to change in an update, but the format from
// the server is unlikely to change.  Internalized versions of the same data are
// provided in DaemonState.
//
// Some data here is not fetched but instead calculated by the daemon
// (latencies, winDnscacheOriginalPath, service quality data).  We usually
// preserve these on updates, but they can be discarded with little impact if
// the format changes.
class COMMON_EXPORT DaemonData : public NativeJsonObject
{
    Q_OBJECT
public:
    DaemonData();

    // Latency measurements (by location ID).  These are restored when the
    // daemon is started, but any new measurements will replace the cached
    // values.
    JsonField(LatencyMap, modernLatencies, {})

    // Cached regions lists.  This is the exact JSON content from the actual
    // regions list; it hasn't been digested or interpreted by the daemon.  This
    // works well when the daemon is upgraded; the API format doesn't change so
    // we can re-interpret the existing cache even if the internal model has
    // changed.
    JsonField(QJsonArray, cachedModernShadowsocksList, {})
    JsonField(QJsonObject, cachedModernRegionsList, {})

    JsonField(QJsonObject, modernRegionMeta, {})

    // Persistent caches of the version advertised by update channel(s).  This
    // is mainly provided to provide consistent UX if the client/daemon are
    // restarted while an update is available (they restore the same "update
    // available" state they had when shut down; they don't come up with no
    // update available and then detect it as a new update again).
    //
    // These are only used to restore the UpdateDownloader state; they're not
    // used by the client.
    //
    // Note that 1.0.1 and older stored availableVersion/availableVersionUri
    // that were used as both the persistent cache and the available version
    // notification to the client UI.  These are intentionally discarded (by
    // the DiscardUnknownProperties behavior) when updating to the new style;
    // the data do not need to be migrated because they would have advertised
    // the version that was just installed.
    JsonField(QString, gaChannelVersion, {})
    JsonField(QString, gaChannelVersionUri, {})
    JsonField(QString, gaChannelOsRequired, {})
    JsonField(QString, betaChannelVersion, {})
    JsonField(QString, betaChannelVersionUri, {})
    JsonField(QString, betaChannelOsRequired, {})

#if defined(Q_OS_WINDOWS)
    // Original service command for the Dnscache service on Windows.  PIA must
    // temporarily override this in order to suppress the service for split
    // tunnel DNS.
    //
    // Although PIA attempts to restore the original value as soon as possible,
    // it's persisted in case the daemon would crash or for whatever reason be
    // unable to restore the service.  The daemon then recovers if it starts up
    // again.
    //
    // This should only change with an OS update, so we keep the last detected
    // value once it is found for resiliency (we never clear this).
    JsonField(QString, winDnscacheOriginalPath, {})
#endif

    // Flags published in the release channel that we are currently loading
    JsonField(std::vector<QString>, flags, {})

    // In-app communication message
    JsonField(AppMessage, appMessage, {})

    // Service quality events - only used if the user has opted in to providing
    // service quality information back to us (see
    // DaemonSettings::sendServiceQualityEvents).  If that setting is not
    // enabled, these are all empty.

    // Current aggregation ID - needed so we can differentiate a large number of
    // errors from a few users vs. a few errors from a lot of users.  Rotated
    // every 24 hours for privacy (see qualityAggIdRotateTime).
    JsonField(QString, qualityAggId, {})
    // The next time we should rotate serviceAggId (UTC Unix time, ms)
    JsonField(qint64, qualityAggIdRotateTime, 0)
    // Events that have been generated but not sent yet.  We try to send the
    // events when 20 have been batched, but this could have more than 20 events
    // if we weren't able to send them at that time (we'll try again later).
    //
    // This is a deque because we add to the back and remove from the front;
    // events are removed once they're sent, but more events might already have
    // been generated.
    JsonField(std::deque<ServiceQualityEvent>, qualityEventsQueued, {})
    // Events that have been generated and sent in the last 24 hours.  These are
    // retained just so the UI can display them.
    //
    // Deque for the same reason - we add sent events to the back and remove
    // from the front as they roll off.
    JsonField(std::deque<ServiceQualityEvent>, qualityEventsSent, {})

    // Check if a single flag exists on the list of flags
    bool hasFlag (const QString &flag) const;
};


// Class encapsulating 'account' properties of the daemon.  This includes:
// * user's access credentials (auth token, or actual credentials if we are
//   not able to reach the API)
// * cached account information (plan, expiration date, etc., just for display)
// * other unique identifiers - not connected to the account, but that should
//   be protected from other apps (port forward token, dedicated IPs)
//
// The file storing these data is accessible only to Administrators (Windows) /
// root (Mac/Linux).
class COMMON_EXPORT DaemonAccount : public NativeJsonObject
{
    Q_OBJECT

public:
    // Several properties are sensitive (passwords, etc.) and are not needed by
    // clients.  Daemon does not send these properties to clients at all.
    static const std::unordered_set<QString> &sensitiveProperties();

public:
    DaemonAccount();

    static bool validateToken(const QString& token);

    // Convenience member to denote whether we are logged in or not.
    // While serialized, it gets reset every startup.
    JsonField(bool, loggedIn, false)

    // The PIA username
    JsonField(QString, username, {})
    // The PIA password (to be replaced with an auth token instead)
    JsonField(QString, password, {})
    // The PIA authentication token (replaces the password)
    JsonField(QString, token, {}, validateToken)

    // The following fields directly map to fetched account info.
    JsonField(QString, plan, {})
    JsonField(bool, active, false)
    JsonField(bool, canceled, false)
    JsonField(bool, recurring, false)
    JsonField(bool, needsPayment, false)
    JsonField(int, daysRemaining, 0)
    JsonField(bool, renewable, false)
    JsonField(QString, renewURL, {})
    JsonField(quint64, expirationTime, 0)
    JsonField(bool, expireAlert, false)
    JsonField(bool, expired, false)

    // These two fields denote what is passed to OpenVPN authentication.
    JsonField(QString, openvpnUsername, {})
    JsonField(QString, openvpnPassword, {})

    // Port forwarding token and signature used for port forward requests in the
    // modern infrastructure.  Collectively, these are the "port forwarding
    // token".
    JsonField(QString, portForwardPayload, {})
    JsonField(QString, portForwardSignature, {})

    // Dedicated IP tokens and the information most recently fetched from the
    // API for those tokens.  These are in DaemonAccount because the token is
    // functionally a password and should be protected like account information,
    // but they are not erased on a logout.
    JsonField(std::vector<AccountDedicatedIp>, dedicatedIps, {})
};

class COMMON_EXPORT SplitTunnelSubnetRule : public NativeJsonObject
{
    Q_OBJECT

public:
    SplitTunnelSubnetRule();
    SplitTunnelSubnetRule(const SplitTunnelSubnetRule &other) {*this = other;}
    SplitTunnelSubnetRule &operator=(const SplitTunnelSubnetRule &other)
    {
        subnet(other.subnet());
        mode(other.mode());
        return *this;
    }

    bool operator==(const SplitTunnelSubnetRule &other) const
    {
        return mode() == other.mode() && subnet() == other.subnet();
    }
    bool operator!=(const SplitTunnelSubnetRule &other) const {return !(*this == other);}

    QString normalizedSubnet() const {
        auto subnetPair = QHostAddress::parseSubnet(subnet());
        return subnetPair.first.isNull() ? "" : QStringLiteral("%1/%2").arg(subnetPair.first.toString()).arg(subnetPair.second);
    }

    QAbstractSocket::NetworkLayerProtocol protocol() const { return QHostAddress::parseSubnet(subnet()).first.protocol(); }

    JsonField(QString, subnet, {})
    JsonField(QString, mode, QStringLiteral("exclude"))
};

class COMMON_EXPORT SplitTunnelRule : public NativeJsonObject
{
    Q_OBJECT

public:
    SplitTunnelRule();
    SplitTunnelRule(const SplitTunnelRule &other) {*this = other;}
    SplitTunnelRule &operator=(const SplitTunnelRule &other)
    {
        path(other.path());
        linkTarget(other.linkTarget());
        mode(other.mode());
        return *this;
    }
    SplitTunnelRule(const QString &_path, const QString &_linkTarget, const QString &_mode) {
        path(_path);
        linkTarget(_linkTarget);
        mode(_mode);
    }

    bool operator==(const SplitTunnelRule &other) const
    {
        return mode() == other.mode() && linkTarget() == other.linkTarget() && path() == other.path();
    }
    bool operator!=(const SplitTunnelRule &other) const {return !(*this == other);}

    // Path to the item selected for this rule.  The selection made by the user
    // is stored directly.  For example:
    // - on Windows this may be a Start menu shortcut (enumerated) or path to an
    //   executable (browsed manually)
    // - on Mac it is a path to an app bundle (either enumerated or browsed
    //   manually)
    // - on Linux it is a path to an executable (browsed manually)
    //
    // On most platforms we have to do additional work to figure out what
    // exactly will be used for the rule (follow links, enumerate helper
    // executables, etc.)  That's done when the firewall rules are applied,
    // rather than when the selection is made, so we can update that logic in
    // the future.
    JsonField(QString, path, {})
    // On Windows only, if the path identifies a shortcut, target is the target
    // of that shortcut as resolved by the user that added the rule.
    //
    // Many apps on Windows install both their shortcut and executable to the
    // user's local AppData, which would prevent the daemon from resolving the
    // link (even with SLGP_RAWPATH, the system still seems to remap it to the
    // service account's AppData somehow).
    //
    // This field isn't used for manually-browsed executables, and it isn't used
    // on any other platform.
    JsonField(QString, linkTarget, {})
    // The behavior to apply for this rule.  The terminology is historical:
    // - "exclude" = Bypass VPN (exclude from VPN)
    // - "include" = Only VPN
    JsonField(QString, mode, QStringLiteral("exclude"), { "exclude", "include" })
};

// A single manual server can be specified in DaemonSettings - this is a dev
// tool only to facilitate testing specific servers.  This becomes a region with
// ID "manual".
//
// An IP and CN must be specified for the region to be shown.  Service groups
// and corresponding region ID are optional.
//
// This is similar to the way Dedicated IP servers are integrated into the
// regions model, although manual servers use regular token auth (not DIP auth).
class COMMON_EXPORT ManualServer : public NativeJsonObject
{
    Q_OBJECT
public:
    ManualServer() {}
    ManualServer(const ManualServer &other) {*this = other;}
    ManualServer &operator=(const ManualServer &other)
    {
        ip(other.ip());
        cn(other.cn());
        openvpnNcpSupport(other.openvpnNcpSupport());
        openvpnUdpPorts(other.openvpnUdpPorts());
        openvpnTcpPorts(other.openvpnTcpPorts());
        serviceGroups(other.serviceGroups());
        correspondingRegionId(other.correspondingRegionId());
        return *this;
    }
    bool operator==(const ManualServer &other) const
    {
        return ip() == other.ip() && cn() == other.cn() &&
            openvpnNcpSupport() == other.openvpnNcpSupport() &&
            openvpnUdpPorts() == other.openvpnUdpPorts() &&
            openvpnTcpPorts() == other.openvpnTcpPorts() &&
            serviceGroups() == other.serviceGroups() &&
            correspondingRegionId() == other.correspondingRegionId();
    }
    bool operator!=(const ManualServer &other) const {return !(*this == other);}

    // The manual server's IP address.  This is used for all services included
    // in the service groups.  This should be a valid IPv4 address, but invalid
    // addresses can still be added to the regions list (as with normal regions
    // from the servers list).
    JsonField(QString, ip, {})
    // The certificate CN to expect from this server.
    JsonField(QString, cn, {})
    // Whether to use NCP.  (If this is false, pia-signal-settings is used to
    // indicate ciphers.)  See Server::openvpnNcpSupport().
    JsonField(bool, openvpnNcpSupport, false)
    // Override OpenVPN UDP/TCP ports for this server.
    // If set, this overrides the ports for the OpenVpnTcp / OpenVpnUdp
    // services for this server.  If these are empty, the corresponding port
    // list from the server list group is used.
    JsonField(std::vector<quint16>, openvpnUdpPorts, {})
    JsonField(std::vector<quint16>, openvpnTcpPorts, {})
    // The service groups from the servers list to apply to this server.  If
    // not specified, this defaults to the current set of service groups
    // excluding "meta", which are:
    //  "ovpntcp", "ovpnudp", "wg"
    JsonField(std::vector<QString>, serviceGroups, {})
    // The corresponding PIA region - if specified, 'meta' servers from this
    // region are used for the manual region.  This is optional; if it is not
    // given, then the region does not have any known 'meta' addresses.
    JsonField(QString, correspondingRegionId, {})
};

// Automation rules
//
// Automation rules are composed of a "condition" and an "action".  The
// condition determines when the rule applies, and at that point the action is
// applied.
class COMMON_EXPORT AutomationRuleAction : public NativeJsonObject,
    public DebugTraceable<AutomationRuleAction>
{
    Q_OBJECT
public:
    AutomationRuleAction() {}
    AutomationRuleAction(const AutomationRuleAction &other) {*this = other;}
    AutomationRuleAction &operator=(const AutomationRuleAction &other)
    {
        connection(other.connection());
        return *this;
    }

    bool operator==(const AutomationRuleAction &other) const
    {
        return connection() == other.connection();
    }
    bool operator!=(const AutomationRuleAction &other) const {return !(*this == other);}

public:
    void trace(QDebug &dbg) const;

    // What to do with the VPN connection when this rule applies:
    // - "enable" - enable the VPN (connect)
    // - "disable" - disable the VPN (disconnect)
    JsonField(QString, connection, "enable", { "enable", "disable" })
};

// Conditions have both a "network type" and "SSID" to implement the different
// types of rules created from the UI.  Empty or 'null' for any criterion means
// any network matches.
//
// The UI does not allow setting more than one criterion on a given condition
// (only one criterion can be non-null).  The backend applies all non-null
// criteria; if more than one was non-null then they would all have to match to
// trigger a rule.
class COMMON_EXPORT AutomationRuleCondition : public NativeJsonObject,
    public DebugTraceable<AutomationRuleCondition>
{
    Q_OBJECT

public:
    AutomationRuleCondition() {}
    AutomationRuleCondition(const AutomationRuleCondition &other) {*this = other;}
    AutomationRuleCondition &operator=(const AutomationRuleCondition &other)
    {
        ruleType(other.ruleType());
        ssid(other.ssid());
        return *this;
    }

    bool operator==(const AutomationRuleCondition &other) const
    {
        return ruleType() == other.ruleType() &&
            ssid() == other.ssid();
    }
    bool operator!=(const AutomationRuleCondition &other) const {return !(*this == other);}

public:
    void trace(QDebug &dbg) const;

    // Rule type - determines what networks this condition matches
    // - "openWifi" - any unencrypted Wi-Fi network
    // - "protectedWifi" - any encrypted Wi-Fi network
    // - "wired" - any wired network
    // - "ssid" - Wi-Fi network with specified SSID (AutomationCondition::ssid())
    //
    // In some cases, the current network may be of a type that's not
    // supported (such as mobile data, Bluetooth or USB tethering to a phone,
    // etc.)  No network type is currently defined for these networks.
    JsonField(QString, ruleType, {}, { "openWifi", "protectedWifi", "wired", "ssid" })

    // Wireless SSID - if set, only wireless networks with the given SSID will
    // match.
    //
    // Only SSIDs that are printable UTF-8 or Latin-1 can be matched, other
    // SSIDs cannot be represented as text.  We do not distinguish between
    // UTF-8 and Latin-1 encodings of the same text.
    // See NetworkConnection::ssid().
    JsonField(QString, ssid, {})
};

class COMMON_EXPORT AutomationRule : public NativeJsonObject,
    public DebugTraceable<AutomationRule>
{
    Q_OBJECT

public:
    AutomationRule() {}
    AutomationRule(const AutomationRule &other) {*this = other;}
    AutomationRule &operator=(const AutomationRule &other)
    {
        condition(other.condition());
        action(other.action());
        return *this;
    }

    bool operator==(const AutomationRule &other) const
    {
        return condition() == other.condition() &&
            action() == other.action();
    }
    bool operator!=(const AutomationRule &other) const {return !(*this == other);}

public:
    void trace(QDebug &dbg) const;

    JsonObjectField(AutomationRuleCondition, condition, {})
    JsonObjectField(AutomationRuleAction, action, {})
};

// Class encapsulating 'settings' properties of the daemon; these are the
// only properties the user is allowed to directly manipulate, and describe
// all configurable preferences, such as the preferred region or encryption
// settings. Note that some preferences are client-specific (such as 'run
// on startup'), and are not saved by the daemon.
//
class COMMON_EXPORT DaemonSettings : public NativeJsonObject
{
    Q_OBJECT

private:
    static const QString defaultReleaseChannelGA;
    static const QString defaultReleaseChannelBeta;

public:
    // Default value for debugLogging when enabling logging.
    static const QStringList defaultDebugLogging;
    // Several settings aren't reset by "reset all settings" for various
    // reasons (things that aren't really presented as "settings", and a few
    // settings that are specifically for troubleshooting).
    static const std::unordered_set<QString> &settingsExcludedFromReset();
    // Same value for QML as a QJsonValue.  (Note: QML uses an invokable method
    // rather than a property to avoid interfering with resetSettings.)
    Q_INVOKABLE QJsonValue getDefaultDebugLogging();

public:
    DaemonSettings();

    typedef JsonVariant<QString, QStringList> DNSSetting;
    static bool validateDNSSetting(const DNSSetting& setting);

    // The daemon version; updated on startup.  Empty prior to ~v1.1.
    JsonField(QString, lastUsedVersion, QStringLiteral(""))

    // This is the location currently selected by the user (as a region ID), or
    // 'auto'.  This location may or may not actually exist.  The client should
    // probably never display this location; instead use one of the location
    // values expressed in DaemonState.  (The client should only use this to set
    // a new choice.)
    JsonField(QString, location, QStringLiteral("auto"))
    // Whether to include geo-only locations.
    JsonField(bool, includeGeoOnly, true)
    // The method used to connect to the VPN
    JsonField(QString, method, QStringLiteral("openvpn"), { "openvpn", "wireguard" })
    JsonField(QString, protocol, QStringLiteral("udp"), { "udp", "tcp" })
    JsonField(QString, killswitch, QStringLiteral("auto"), { "on", "off", "auto" })
    // Whether to use the VPN as the default route.  This is a split tunnel
    // setting (due to the non-default route being applied as part of the split
    // tunnel implementation on Mac); it is only used when splitTunnelEnabled is
    // true.
    // Prior versions of the PIA client had a "routeDefault" setting; the
    // setting was renamed when it was implemented properly to avoid unexpected
    // behavior on downgrades.
    JsonField(bool, defaultRoute, true)
    JsonField(bool, routedPacketsOnVPN, true)
    JsonField(bool, blockIPv6, true) // block IPv6 traffic
    JsonField(DNSSetting, overrideDNS, QStringLiteral("pia"), validateDNSSetting) // use PIA DNS servers (symbolic name, array with 1-2 IPs, or empty string to use existing DNS)
    JsonField(bool, allowLAN, true) // permits LAN traffic when connected/killswitched
    JsonField(bool, portForward, false) // forward a port through the VPN tunnel, see DaemonState::forwardedPort
    JsonField(bool, enableMACE, false) // Enable MACE Ad tracker
    JsonField(uint, remotePortUDP, 0) // 0 == auto
    JsonField(uint, remotePortTCP, 0) // 0 == auto
    JsonField(uint, localPort, 0) // 0 == auto
    JsonField(uint, mtu, 0) // 0 == unspecified
    JsonField(QString, cipher, QStringLiteral("AES-128-GCM"), { "AES-128-GCM", "AES-256-GCM" })
    // On Windows, the method to use to configure the TAP adapter's IP addresses
    // and DNS servers.
    // - dhcp - use "ip-win32 dhcp", DNS servers applied as DHCP options
    // - static - use "ip-win32 netsh", DNS servers applied using netsh
    // No method works reliably on Windows.  The DHCP negotation can time out or
    // be blocked, which results in the TAP adapter getting an automatic local
    // address.  netsh is prone to spurious failures ("The parameter is
    // incorrect"), and it depends on the DNS Client service to apply DNS
    // servers, which some users disable.
    JsonField(QString, windowsIpMethod, QStringLiteral("dhcp"), {"dhcp", "static"})

    // Proxy setting
    //  - "none" - No proxy
    //  - "custom" - Use proxyCustom
    //  - "shadowsocks" - Use a PIA shadowsocks region - proxyShadowsocksLocation
    // May have additional values in the future, such as using a PIA region.
    JsonField(QString, proxy, QStringLiteral("none"), {"none", "custom", "shadowsocks"})

    // Custom proxy - used when proxy is "custom", persisted even when "none" is
    // selected
    JsonField(CustomProxy, proxyCustom, {})

    // Shadowsocks proxy location - used when proxy is "shadowsocks", identifies
    // a PIA region or 'auto'.  Invalid locations are treated as 'auto'.
    JsonField(QString, proxyShadowsocksLocation, QStringLiteral("auto"))

    // Automatically try alternate transport settings if the selected protocol/
    // port does not work.
    JsonField(bool, automaticTransport, true)

    // Specify debug logging filter rules (null = disable logging to file)
    JsonField(Optional<QStringList>, debugLogging, nullptr)

    // The "GA release" update channel from which we retrieve updates, such as
    // "release", "qa_release", etc.  Valid values are determined by the update
    // channels listed in the version metadata.
    //
    // An empty string disables checking for updates.  Otherwise, this channel
    // is always checked and used to offer updates.
    JsonField(QString, updateChannel, defaultReleaseChannelGA)
    // The "Beta release" update channel from which we retrieve updates, such as
    // "beta", "qa_beta", etc.  As with updateChannel, can be any channel listed
    // in version metadata.
    //
    // Updates from this channel are only offered if beta updates are enabled
    // and it is not empty.
    // This channel may be checked though even if it's not being used to offer
    // updates, such as to show an available beta in the Settings window.
    JsonField(QString, betaUpdateChannel,defaultReleaseChannelBeta)
    // Whether the user wants beta updates.
    JsonField(bool, offerBetaUpdates, false)
    // Whether to store and send service quality information.
    JsonField(bool, sendServiceQualityEvents, false)

    // Whether split tunnel is enabled
    JsonField(bool, splitTunnelEnabled, false)
    // Whether to also split DNS traffic
    JsonField(bool, splitTunnelDNS, true)
    // Rules for excluding/including apps from VPN
    JsonField(QVector<SplitTunnelRule>, splitTunnelRules, {})

    // Subnets (both ipv4 and ipv6) to exclude from VPN
    JsonField(QVector<SplitTunnelSubnetRule>, bypassSubnets, {})

    // Whether automation rules are enabled or not.
    JsonField(bool, automationEnabled, {})
    // Automation rules.  Initially there are no rules (not even general rules,
    // these can be created by the user).
    JsonField(std::vector<AutomationRule>, automationRules, {})

    // Whether to use the WireGuard kernel module on Linux, if it's available.
    // If false, uses wireguard-go method instead, even if the kernel module is
    // available.
    JsonField(bool, wireguardUseKernel, true)

    // If no data is received for (wireguardPingTimeout/2) seconds, fire off a ping.
    // If no data is recieved for another (wireguardPingTimeout/2) seconds, assume that the connection
    // is lost
    //
    // Should be a multiple of statsInterval (5)
    JsonField(uint, wireguardPingTimeout, 60)

    // These settings are legacy and have been moved to client-side settings.
    // They're still present in DaemonSettings so the client can migrate them.
    JsonField(bool, connectOnLaunch, false) // Connect when first client connects
    JsonField(bool, desktopNotifications, true) // Show desktop notifications
    JsonField(QVector<QString>, favoriteLocations, {})
    JsonField(QStringList, recentLocations, {})
    JsonField(QString, themeName, QStringLiteral("dark"))
    JsonField(QVector<QString>, primaryModules, QVector<QString>::fromList({QStringLiteral("region"), QStringLiteral("ip")}))
    JsonField(QVector<QString>, secondaryModules, QVector<QString>::fromList({QStringLiteral("quickconnect"), QStringLiteral("performance"), QStringLiteral("usage"), QStringLiteral("settings"), QStringLiteral("account")}))

    JsonField(bool, persistDaemon, false)

    JsonField(QString, macStubDnsMethod, QStringLiteral("NX"), {"NX", "Refuse", "0.0.0.0"})

    // Store the number of attempted sessions used for various metrics
    JsonField(int, sessionCount, 0)

    // Set to false when user opts out, or finishes rating successfully.
    JsonField(bool, ratingEnabled, true)

    // Keep track of the id of the last dismissed in-app message
    JsonField(quint64, lastDismissedAppMessageId, 0)

    // Keep extra logs on disk for debugging/diagnosis purposes. This is disabled
    // by default and can only be turned on using the CLI
    JsonField(bool, largeLogFiles, false)

    // Whether to show in-app communication messages to the user
    JsonField(bool, showAppMessages, true)

    // Manual server dev setting - for testing specific servers
    JsonField(ManualServer, manualServer, {})
};

// Compare QSharedPointer<Location>s by value; used by ServiceLocations
// and ConnectionInfo
inline bool compareLocationsValue(const QSharedPointer<Location> &pFirst,
                                  const QSharedPointer<Location> &pSecond)
{
    // If one is nullptr, they're only the same if they're both nullptr
    if(!pFirst || !pSecond)
        return pFirst == pSecond;
    return *pFirst == *pSecond;
}

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
class COMMON_EXPORT ServiceLocations : public NativeJsonObject
{
    Q_OBJECT

public:
    ServiceLocations() {}
    ServiceLocations(const ServiceLocations &other) {*this = other;}

public:
    ServiceLocations &operator=(const ServiceLocations &other)
    {
        chosenLocation(other.chosenLocation());
        bestLocation(other.bestLocation());
        nextLocation(other.nextLocation());
        return *this;
    }

    bool operator==(const ServiceLocations &other) const
    {
        return compareLocationsValue(chosenLocation(), other.chosenLocation()) &&
            compareLocationsValue(bestLocation(), other.bestLocation()) &&
            compareLocationsValue(nextLocation(), other.nextLocation());
    }
    bool operator!=(const ServiceLocations &other) const
    {
        return !(*this == other);
    }

public:
    // The daemon's interpretation of the selected location.  If
    // DaemonSettings::location is a valid non-auto location, this is that
    // location's object.  Otherwise, it is undefined, which indicates 'auto'.
    //
    // (The daemon interprets an invalid location as 'auto'.)
    JsonField(QSharedPointer<Location>, chosenLocation, {})
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
    JsonField(QSharedPointer<Location>, bestLocation, {})
    // The next location we would connect to if the user chooses to connect (or
    // reconnect) right now.
    //
    // Like 'chosenLocation', undefined if and only if no locations are known.
    JsonField(QSharedPointer<Location>, nextLocation, {})
};

// Information about the current ongoing connection and the last successful
// connection.  See DaemonState::connectingConfig and connectedConfig.
// Note that this doesn't provide the complete settings used to connect.  It
// provides most fields from ConnectionConfig, but in particular, the custom
// proxy username/password are omitted.
//
// vpnLocation and proxyShadowsocks are not necessarily locations from that
// list.  (These do not point to the same objects that were in that list -
// they're copied from the list since the represent the data we used or are
// using to establish a a connection, even if the data in the locations list
// changes later.)
class COMMON_EXPORT ConnectionInfo : public NativeJsonObject
{
    Q_OBJECT

public:
    ConnectionInfo() {}
    ConnectionInfo(const ConnectionInfo &other) { *this = other; }

    const ConnectionInfo &operator=(const ConnectionInfo &other)
    {
        vpnLocation(other.vpnLocation());
        vpnLocationAuto(other.vpnLocationAuto());
        method(other.method());
        methodForcedByAuth(other.methodForcedByAuth());
        dnsType(other.dnsType());
        openvpnCipher(other.openvpnCipher());
        otherAppsUseVpn(other.otherAppsUseVpn());
        proxy(other.proxy());
        proxyCustom(other.proxyCustom());
        proxyShadowsocks(other.proxyShadowsocks());
        proxyShadowsocksLocationAuto(other.proxyShadowsocksLocationAuto());
        portForward(other.portForward());
        return *this;
    }

    bool operator==(const ConnectionInfo &other) const
    {
        // Compare the locations by value
        return compareLocationsValue(vpnLocation(), other.vpnLocation()) &&
            vpnLocationAuto() == other.vpnLocationAuto() &&
            method() == other.method() &&
            methodForcedByAuth() == other.methodForcedByAuth() &&
            dnsType() == other.dnsType() &&
            openvpnCipher() == other.openvpnCipher() &&
            otherAppsUseVpn() == other.otherAppsUseVpn() &&
            proxy() == other.proxy() && proxyCustom() == other.proxyCustom() &&
            compareLocationsValue(proxyShadowsocks(), other.proxyShadowsocks()) &&
            proxyShadowsocksLocationAuto() == other.proxyShadowsocksLocationAuto() &&
            portForward() == other.portForward();
    }

    bool operator!=(const ConnectionInfo &other) const { return !(*this == other); }

public:
    // The VPN location used for this connection.  Follows the truth table in
    // DaemonState.
    JsonField(QSharedPointer<Location>, vpnLocation, {})
    // Whether the VPN location was an automatic selection
    JsonField(bool, vpnLocationAuto, false)

    // The VPN method used for this connection
    JsonField(QString, method, QStringLiteral("openvpn"), {"openvpn", "wireguard"})
    // Whether the VPN method was forced to OpenVPN due to lack of an auth token
    JsonField(bool, methodForcedByAuth, false)

    // DNS type used for this connection
    JsonField(QString, dnsType, QStringLiteral("pia"), {"pia", "handshake", "local", "existing", "custom"})

    // OpenVPN cryptographic settings - only meaningful when method is OpenVPN
    JsonField(QString, openvpnCipher, {})

    // Whether the default app behavior is to use the VPN - i.e. whether apps
    // with no specific rules use the VPN (always true when split tunnel is
    // disabled).
    JsonField(bool, otherAppsUseVpn, true)
    // The proxy type used for this connection - same values as
    // DaemonSettings::proxy (when set)
    JsonField(QString, proxy, {})
    // If proxy is 'custom', the custom proxy hostname:port that were used
    JsonField(QString, proxyCustom, {})
    // If proxy is 'shadowsocks', the Shadowsocks location that was used
    JsonField(QSharedPointer<Location>, proxyShadowsocks, {})
    // Whether the Shadowsocks location was an automatic selection
    JsonField(bool, proxyShadowsocksLocationAuto, false)

    // Whether port forwarding is enabled for this connection
    JsonField(bool, portForward, false)
};

// Class encapsulating 'state' properties of the daemon; these describe
// the current state of the daemon and the VPN connection, and are not
// saved to disk. These are combined with the 'data' object when passed
// to the client.
//
class COMMON_EXPORT DaemonState : public NativeJsonObject
{
    Q_OBJECT

public:
    // Installation states for the network extension (WFP callout on
    // Windows, network kernel extension on Mac).
    enum class NetExtensionState
    {
        // We couldn't determine the state.  Can happen due to errors (failure
        // to open the SCM on Windows, etc.) or because there's no way to test
        // the state (initial state on Mac OS).
        Unknown,
        // It isn't installed.
        NotInstalled,
        // It is installed.
        Installed,
    };
    Q_ENUM(NetExtensionState)

    // Special values for the value of forwardedPort.  Note that these names are
    // part of the CLI interface for "get portforward" and shouldn't be changed.
    enum PortForwardState : int
    {
        // PF not enabled, or not connected to VPN
        Inactive = 0,
        // Enabled, connected, and supported - requesting port
        Attempting = -1,
        // Port forward failed
        Failed = -2,
        // PF enabled, but not available for the connected region
        Unavailable = -3,
    };
    Q_ENUM(PortForwardState)

public:
    DaemonState();

    // Let the client know whether we currently have an auth token; the client
    // uses this to detect the "logged in, but API unreachable" state (where we
    // will try to connect to the VPN server using credential auth).  The client
    // can't access the actual auth token.
    JsonField(bool, hasAccountToken, false)

    // Boolean indicating whether the user wants to be connected or not.
    // This specifically tracks the user's intent - this should _only_ ever be
    // changed due to a user request to connect or disconnect.
    //
    // In general, any connection state can occur for any value of vpnEnabled.
    // It is even possible to be "Disconnected" while "vpnEnabled == true"; this
    // happens if a fatal error causes a reconnection to abort.  In this case
    // we correctly have vpnEnabled=true, because the user intended to be
    // connected, but the app cannot try to connect due to the fatal error.
    JsonField(bool, vpnEnabled, false)
    // The current actual state of the VPN connection.
    JsonField(QString, connectionState, QStringLiteral("Disconnected"))
    // When in a connecting state, enabled when enough attempts have been made
    // to trigger the 'slow' attempt intervals.  Resets to false before going
    // to a non-connecting state, or when settings change during a series of
    // attempts.
    JsonField(bool, usingSlowInterval, false)
    // Boolean indicating whether a reconnect is needed in order to apply settings changes.
    JsonField(bool, needsReconnect, false)
    // Total number of bytes received over the VPN.
    JsonField(uint64_t, bytesReceived, 0)
    // Total number of bytes sent over the VPN.
    JsonField(uint64_t, bytesSent, 0)
    // When DaemonSettings::portForward has been enabled, the port that was
    // forwarded.  Positive values are a forwarded port; other values are
    // special values from PortForwardState.
    JsonField(int, forwardedPort, PortForwardState::Inactive)
    // External non-VPN IP address detected before connecting to the VPN
    JsonField(QString, externalIp, {})
    // External VPN IP address detected after connecting
    JsonField(QString, externalVpnIp, {})

    // These are the transport settings that the user chose, and the settings
    // that we actually connected with.  They are provided in the Connected
    // state.
    //
    // The client mainly uses these to detect whether the chosen and actual
    // transports are different.  If a connection is successfully made with
    // alternate settings, the client will indicate the specific values used in
    // the UI.
    //
    // chosenTransport.port will only be zero when a different protocol is used
    // for actualTransport.  If the protocols are the same, and the default port
    // was selected, then chosenTransport.port is set to the actual default port
    // for the selected server (so the client can tell if it matches the actual
    // transport).
    JsonField(Optional<Transport>, chosenTransport, {})
    JsonField(Optional<Transport>, actualTransport, {})

    // Service locations chosen by the daemon, based on the chosen and best
    // locations, etc.
    //
    // All location choices are provided for the VPN service and for the
    // Shadowsocks service.  The logic for determining each one is different
    // ("auto" means different things, for example), but the meaning of each
    // field is the same ("the next location we would use", "the location we
    // would use for auto", etc.)
    JsonObjectField(ServiceLocations, vpnLocations, {})
    JsonObjectField(ServiceLocations, shadowsocksLocations, {})

    // Information about the current connection attempt and/or last established
    // connection.  Includes VPN locations and proxy configuration.
    //
    // The validity of these data depends on the current state.  ('Valid' means
    // the ConnectionInfo has a valid VPN location, and that the other setting
    // information is meaningful.)
    //
    // (X = valid, - = not valid, ? = possibly valid)
    // State                    | connectingConfig | connectedConfig
    // -------------------------+------------------+-----------------
    // Disconnected             | -                | ?
    // Connecting               | X                | -
    // Connected                | -                | X
    // Interrupted              | X                | X
    // Reconnecting             | X                | X
    // DisconnectingToReconnect | X                | ?
    // Disconnecting            | -                | ?
    //
    // The validity of 'connectedConfig' in Disconnected, Disconnecting and
    // DisconnectingToReconnect depends on whether we had a connection prior to
    // entering that state.
    //
    // Note that Interrupted and Reconnecting both only occur after a
    // successful connection, so connectedLocation is always valid in those
    // states and represents the last successful connection.
    JsonObjectField(ConnectionInfo, connectingConfig, {})
    JsonObjectField(ConnectionInfo, connectedConfig, {})
    // The next configuration we would use to connect if we connected right now.
    // This is set based on the current settings, regions list, etc.
    JsonObjectField(ConnectionInfo, nextConfig, {})

    // The specific server that we have connected to, when connected (only
    // provided in the Connected state)
    JsonField(Optional<Server>, connectedServer, {})

    // Available regions, mapped by region ID.  These are from either the
    // current or new regions list.  This includes both dedicated IP regions and
    // regular regions, which are treated the same way by most logic referring
    // to regions.
    JsonField(LocationsById, availableLocations, {})

    // Locations grouped by country and sorted by latency.  The locations are
    // chosen from the active infrastructure specified by the "infrastructure"
    // setting.
    //
    // This is used for display purposes - in regions lists, in the CLI
    // "get regions", etc.  It does _not_ include dedicated IP regions, which
    // are handled differently in display contexts (those are in
    // dedicatedIpLocations)
    //
    // This is provided by the daemon to ensure that the client and daemon
    // handle these in exactly the same way.  Although Daemon itself only
    // technically cares about the lowest-latency location, the entire list must
    // be sorted for display in the regions list.
    //
    // The countries are sorted by the lowest latency of any location in the
    // country (which ensures that the lowest-latency location's country is
    // first).  Ties are broken by country code.
    JsonField(std::vector<CountryLocations>, groupedLocations, {})

    // Dedicated IP locations sorted by latency with the same tie-breaking logic
    // as groupedLocations().  This is used in display contexts alongside
    // groupedLocations(), as dedicated IP regions are displayed differently.
    JsonField(std::vector<QSharedPointer<Location>>, dedicatedIpLocations, {})

    // All supported ports for the OpenVpnUdp and OpenVpnTcp services in the
    // active infrastructure (union of the supported ports among all advertised
    // servers).  This can be derived from the regions lists above, but this
    // derivation is relatively complex so these are stored.
    //
    // This is just used to define the choices presented in the "Remote Port"
    // drop-down.
    JsonField(DescendingPortSet, openvpnUdpPortChoices, {})
    JsonField(DescendingPortSet, openvpnTcpPortChoices, {})

    // Per-interval bandwidth measurements while connected to the VPN.  Only a
    // limited number of intervals are kept (new values past the limit will bump
    // off the oldest value).  Older values are first.
    //
    // When not connected, this is an empty array.
    JsonField(QList<IntervalBandwidth>, intervalMeasurements, {})
    // Timestamp when the VPN connection was established - ms since system
    // startup, using a monotonic clock.  0 if we are not connected.
    //
    // Monotonic time is used so that changes in the wall-clock time won't
    // affect the computed duration.  However, monotonic time usually excludes
    // time while the system is sleeping/hibernating.  Most of the time, this
    // will force us to reconnect anyway, but if the system sleeps for a short
    // enough time that the connection is still alive, it is not too surprising
    // that the connection duration would exclude the sleep time.
    JsonField(qint64, connectionTimestamp, {})

    // These fields all indicate errors/warnings/notification conditions
    // detected by the Daemon that can potentially be displayed in the client.
    // The actual display semantics, including the message localization and
    // whether the user can dismiss the condition, are handled by the client.
    //
    // Several of these are reported as timestamps so the client can observe
    // when the problem recurs and re-show the notification if it was dismissed.
    // Timestamps are handled as the number of milliseconds since 01-01-1970
    // 00:00 UTC.  (Qt has a Date type in QML, but it's more cumbersome than a
    // plain count for general use.)  0 indicates that the condition does not
    // currently apply.

    // Testing override(s) were present, but could not be loaded (invalid JSON,
    // etc.).  This is set when the daemon activates, and it can be updated if
    // the daemon deactivates and then reactivates.  It's a list of
    // human-readable names for the resources that are overridden (not
    // localized, this is intended for testing only).
    JsonField(QStringList, overridesFailed, {})
    // Testing override(s) are active.  Human-readable names of the overridden
    // features; set at daemon startup, like overridesFailed.
    JsonField(QStringList, overridesActive, {})
    // Authorization failed in the OpenVPN connection (timestamp of failure).
    // Note that this does not really mean that the user's credentials are
    // incorrect, see ClientNotifications.qml.
    JsonField(qint64, openVpnAuthFailed, 0)
    // Connection was lost (timestamp)
    JsonField(qint64, connectionLost, 0)
    // Failed to resolve the configured proxy.
    JsonField(qint64, proxyUnreachable, 0)
    // Killswitch rules blocking Internet access are active.  Note that this can
    // apply in the Connecting/Connected states too, but usually shouldn't be
    // displayed in these states.
    JsonField(bool, killswitchEnabled, false)
    // Available update version - set when the newest version advertised on the
    // active release channel(s) is different from the daemon version; empty if
    // no update is available or it is the same version as the daemon.  The
    // client offers to download this version when it's set.
    // Note that the download URI is not provided since it is not used by the
    // client.
    JsonField(QString, availableVersion, {})
    // Enabled if the current OS is out of support - newer updates are available
    // but they do not support this OS version.
    JsonField(bool, osUnsupported, false)
    // When a download has been initiated, updateDownloadProgress indicates the
    // progress (as a percentage).  -1 means no download is occurring,
    // 0-100 indicates that a download is ongoing.  When the download completes,
    // updateInstallerPath is set.
    JsonField(int, updateDownloadProgress, -1)
    // The path to the installer for an update that has been downloaded.  Empty
    // if no installer has been downloaded.
    JsonField(QString, updateInstallerPath, {})
    // If a download attempt fails, updateDownloadFailure is set to the
    // timestamp of the failure.  This is cleared when a new download is
    // attempted.
    JsonField(qint64, updateDownloadFailure, 0)
    // The version of the installer downloaded (when updateInstallerPath is
    // set), being downloaded (when updateDownloadProgress is set), or that
    // failed (when updateDownloadFailure is set)
    JsonField(QString, updateVersion, {})
    // The TAP adapter is missing on Windows (the client offers to reinstall it)
    // Not dismissible, so this is just a boolean flag.
    JsonField(bool, tapAdapterMissing, false)
    // The WinTUN driver is missing on Windows.  Like the TAP error, the client
    // offers to reinstall it, and this is not dismissible.
    JsonField(bool, wintunMissing, false)
    // State of the network extension - the WFP callout on Windows, the
    // network kernel extension on Mac.  See Daemon::NetExtensionState.
    // This extension is currently used for the split tunnel feature but may
    // have other functionality in the future.
    // This causes the client to try to install the driver before enabling the
    // split tunnel setting if necessary, or show warnings if the driver is not
    // installed and the setting is already enabled.
    JsonField(QString, netExtensionState, QStringLiteral("NotInstalled"))
    // Result of the connection test performed after connecting to the VPN.  If
    // the connection is not working, this will be set, and the client will show
    // a warning.
    JsonField(bool, connectionProblem, false)
    // A dedicated IP will expire soon.  When active, the number of days until
    // the next expiration is also given.
    JsonField(quint64, dedicatedIpExpiring, 0)
    JsonField(int, dedicatedIpDaysRemaining, 0)
    // A dedicated IP has changed (as observed by the daemon when refreshing
    // DIP info).  Cleared if the notification is dismissed.
    JsonField(quint64, dedicatedIpChanged, 0)

    // We failed to configure DNS on linux
    JsonField(qint64, dnsConfigFailed, 0)
    // Flag to indicate that the last time a client exited, it was an invalid exit
    // and an message should possibly be displayed
    JsonField(bool, invalidClientExit, false)
    // Flag to indicate that the daemon killed the last client connection.
    // Similar to invalidClientExit, but does not trigger any client warning,
    // since this is normally caused by the OS freezing the client process, and
    // we expect the client process to reconnect.
    JsonField(bool, killedClient, false)

    // hnsd is failing to launch.  Set after it fails for 10 seconds, cleared
    // when it launches successfully and runs for at least 30 seconds.
    // (Timestamp of first warning.)
    JsonField(qint64, hnsdFailing, 0)
    // hnsd is failing to sync (but it is running, or at least it was at some
    // point).  Set if it runs for 5 seconds without syncing a block, cleared
    // once it syncs a block.  This can overlap with hnsdFailing if it also
    // crashes or restarts after this condition occurs.
    JsonField(qint64, hnsdSyncFailure, 0)

    // The original gateway IP address before we activated the VPN
    JsonField(QString, originalGatewayIp, {})

    // The original interface IP and network prefix before we activated the VPN
    JsonField(QString, originalInterfaceIp, {})
    JsonField(unsigned, originalInterfaceNetPrefix, {})

    // The original gateway interface before we activated the VPN
    JsonField(QString, originalInterface, {})

    // The original IPv6 interface IP before we activated the VPN
    JsonField(QString, originalInterfaceIp6, {})

    // The original IPv6 gateway IP before we activated the VPN
    JsonField(QString, originalGatewayIp6, {})

    // The key for the primary service on macOS
    JsonField(QString, macosPrimaryServiceKey, {})

    // A multi-function value to indicate snooze state and
    // -1 -> Snooze not active
    // 0 -> Connection transitioning from "VPN Connected" to "VPN Disconnected" because user requested Snooze
    // >0 -> The monotonic time when the snooze will be ending. Please note this can be in the past, and will be the case when the connection
    // transitions from "VPN Disconnected" to "VPN Connected" once the snooze ends
    JsonField(qint64, snoozeEndTime, -1)

    // If split tunnel is not available, this is set to a list of reasons.
    // The reasons are listed in SettingsMessages.qml along with their UI text.
    // (For example - "libnl_invalid" is set if the libnl libraries can't be
    // loaded on Linux.)
    JsonField(std::vector<QString>, splitTunnelSupportErrors, {})

    // On Mac/Linux, the name of the tunnel device being used.  Set during the
    // [Still](Connecting|Reconnecting) states when known, remains set while
    // connected.  Cleared in the Disconnected state.  In other states, the
    // value depends on whether we had reached this phase of the last connection
    // attempt.
    JsonField(QString, tunnelDeviceName, {})
    JsonField(QString, tunnelDeviceLocalAddress, {})
    JsonField(QString, tunnelDeviceRemoteAddress, {})

    // Whether WireGuard is available at all on this OS.  (False on Windows 7.)
    JsonField(bool, wireguardAvailable, true)
    // Whether a kernel implementation of Wireguard is available (only possible
    // on Linux).
    JsonField(bool, wireguardKernelSupport, false)
    // The DNS servers prior to connecting
    JsonField(std::vector<quint32>, existingDNSServers, {})

    // Automation rules - indicates which rule has triggered, which rule
    // currently matches, the rule that could be created for the current
    // network, etc.

    // If automation rules aren't available, this is set to a list of reasons.
    // The possible reasons are a subset of those used for
    // splitTunnelSupportErrors; the same text from SettingsMessages is used in
    // the UI.
    JsonField(std::vector<QString>, automationSupportErrors, {})

    // The rule that caused the last VPN connection transition, if there is one.
    // If the last transition was manual instead (connect button, snooze, etc.),
    // this is 'null'.
    //
    // This is a complete Rule with both action and condition
    JsonField(Optional<AutomationRule>, automationLastTrigger, {})

    // The rule that matches the current network, even if it didn't cause a
    // transition or the VPN connection was transitioned manually since then.
    // Causes the "connected" indicator to appear in Settings.
    //
    // This is a rule that exists in DaemonSettings::automationRules.  If there
    // is no custom rule for the current network, it can be a general rule if
    // there is one that matches.
    //
    // If there is no network connection, or if the current connection does not
    // match any general rule, it is 'null'.  (This is the case for connections
    // of unknown type, which can't match any rule.)
    JsonField(Optional<AutomationRule>, automationCurrentMatch, {})

    // These are rule conditions for wireless networks that are currently
    // connected.  Note that there can in principle be more than one of these
    // (if the device has multiple Wi-Fi interfaces), and these may not be the
    // default network (if, say, Ethernet is also connected).
    //
    // Conditions are populated for each connected wireless network, even if
    // rules already exist for them.
    JsonField(std::vector<AutomationRuleCondition>, automationCurrentNetworks, {})
};


#if defined(PIA_DAEMON) || defined(UNIT_TEST)

// The 127/8 loopback address used for local DNS.
COMMON_EXPORT const QString resolverLocalAddress();

// The IP used for PIA DNS in the modern infrastructure - on-VPN, and with MACE.
COMMON_EXPORT const QString piaModernDnsVpnMace();
// The IP used for PIA DNS in the modern infrastructure - on-VPN (no MACE).
COMMON_EXPORT const QString piaModernDnsVpn();

#endif

#endif // SETTINGS_H
