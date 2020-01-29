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
#line HEADER_FILE("settings.h")

#ifndef SETTINGS_H
#define SETTINGS_H
#pragma once

#include "json.h"
#include <QVector>

// ShadowsocksServer describes a Shadowsocks endpoint in a location as obtained
// from the Shadowsocks server list.
class COMMON_EXPORT ShadowsocksServer : public NativeJsonObject
{
    Q_OBJECT

public:
    ShadowsocksServer() {}
    ShadowsocksServer(const ShadowsocksServer &other)
    {
        host(other.host());
        port(other.port());
        key(other.key());
        cipher(other.cipher());
    }

public:
    bool operator==(const ShadowsocksServer &other) const
    {
        return host() == other.host() && port() == other.port() &&
               key() == other.key() && cipher() == other.cipher();
    }
    bool operator!=(const ShadowsocksServer &other) const
    {
        return !(*this == other);
    }

    JsonField(QString, host, {})
    JsonField(uint, port, {})
    JsonField(QString, key, {})
    JsonField(QString, cipher, {})
};

// ServerLocation describes a PIA region.  All regions have a VPN endpoint, some
// regions additionally have auxiliary service endpoints.  Latency measurements
// stored by the PIA daemon are also stored here.
class COMMON_EXPORT ServerLocation : public NativeJsonObject
{
    Q_OBJECT

private:
    static QString addressHost(const QString &address);
    static quint16 addressPort(const QString &address);

public:
    ServerLocation() {}
    ServerLocation(const ServerLocation &other)
    {
        id(other.id());
        name(other.name());
        country(other.country());
        dns(other.dns());
        portForward(other.portForward());
        openvpnUDP(other.openvpnUDP());
        openvpnTCP(other.openvpnTCP());
        ping(other.ping());
        serial(other.serial());
        isSafeForAutoConnect(other.isSafeForAutoConnect());
        shadowsocks(other.shadowsocks());   // Share the object since it is not mutated
        latency(other.latency());
    }

    bool operator==(const ServerLocation &other)
    {
        // Check Shadowsocks locations by value
        if(shadowsocks() && other.shadowsocks())
        {
            // Both have SS endpoints, compare by value
            if(*shadowsocks() != *other.shadowsocks())
                return false;   // SS endpoints differ
        }
        else if(!shadowsocks() || !other.shadowsocks())
        {
            // One or both is nullptr.  They differ if one is valid (otherwise,
            // they're both nullptr, so they are equal)
            if(shadowsocks() || other.shadowsocks())
                return false;
        }

        return name() == other.name() && country() == other.country() &&
            dns() == other.dns() && portForward() == other.portForward() &&
            openvpnUDP() == other.openvpnUDP() &&
            openvpnTCP() == other.openvpnTCP() && ping() == other.ping() &&
            serial() == other.serial() &&
            isSafeForAutoConnect() == other.isSafeForAutoConnect() &&
            latency() == other.latency();
    }

    // Region ID - matches the key in ServerLocations.  This is provided by all
    // servers lists and is used to match the data.
    JsonField(QString, id, {})

    // These fields all originate from the VPN servers list.  Currently, all
    // regions must have a VPN server endpoint to be recognized by Desktop,
    // since we need the name, country, etc. from that list, and we need the
    // 'ping' endpoint to measure latency.  If a region only has auxiliary
    // services (Shadowsocks, etc.), it will be ignored.
    //
    // Someday we may relax that restriction and break these fields out into an
    // optional service object.
    JsonField(QString, name, {})
    JsonField(QString, country, {})
    JsonField(QString, dns, {})
    JsonField(bool, portForward, {})
    JsonField(QString, openvpnUDP, {})
    JsonField(QString, openvpnTCP, {})
    JsonField(QString, ping, {})
    JsonField(QString, serial, {})
    JsonField(bool, isSafeForAutoConnect, true)

    // The remaining fields do not come from the VPN servers list.

    // Shadowsocks service endpoint.  Optional - nullptr if this region does not
    // have Shadowsocks.
    // This object should not be mutated once it is applied to a ServerLocation.
    JsonField(QSharedPointer<ShadowsocksServer>, shadowsocks, {})
    // Latency measured by the daemon
    JsonField(Optional<double>, latency, {})

public:
    // Get the host/port parts of the UDP or TCP addresses.  Ports return 0 if
    // there isn't a port in the address from the server for some reason.
    QString udpHost() const {return addressHost(openvpnUDP());}
    QString tcpHost() const {return addressHost(openvpnTCP());}
    quint16 udpPort() const {return addressPort(openvpnUDP());}
    quint16 tcpPort() const {return addressPort(openvpnTCP());}
};
typedef QHash<QString, QSharedPointer<ServerLocation>> ServerLocations;

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

    JsonField(QVector<QSharedPointer<ServerLocation>>, locations, {})
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
    // If this transport has port 0 (default), determine the effective port to
    // use when connecting to 'location'.  (Updates this transport's port.)
    void resolvePort(const ServerLocation &location);
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

// Class encapsulating 'data' properties of the daemon; these are cached
// and persist between daemon instances, and typically determine the
// operating parameters of the PIA service, such as certificates and
// region lists. They are typically received from PIA servers or
// calculated, but can also be hardcoded.
//
class COMMON_EXPORT DaemonData : public NativeJsonObject
{
    Q_OBJECT
public:
    DaemonData();

    typedef QHash<QString, QStringList> CertificateAuthorityMap;

    JsonField(ServerLocations, locations, {})
    JsonField(CertificateAuthorityMap, certificateAuthorities, {})

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
    JsonField(QString, betaChannelVersion, {})
    JsonField(QString, betaChannelVersionUri, {})

    // The supported remote ports for the VPN connection (both udp/tcp)
    JsonField(QVector<uint>, udpPorts,  QVector<uint>({1194, 8080, 9201, 53}))
    JsonField(QVector<uint>, tcpPorts,  QVector<uint>({443, 110, 80}))

public:
    QStringList getCertificateAuthority(const QString& type);
};


// Class encapsulating 'account' properties of the daemon; these are the
// user's account credentials (e.g. authentication token) as well as basic
// information about their account. This provides a single easy container
// to wipe in order to cleanly log out / remove user information.
//
class COMMON_EXPORT DaemonAccount : public NativeJsonObject
{
    Q_OBJECT
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
    JsonField(QString, email, {})

    // These two fields denote what is passed to OpenVPN authentication.
    JsonField(QString, openvpnUsername, {})
    JsonField(QString, openvpnPassword, {})

    // ClientId is a random identifier used for port forward requests (and
    // possibly other things in the future).
    // Although it's not directly tied to the user's account, it does identify
    // the computer, and it makes sense to wipe it with the account credentials.
    // This is a 256-bit number encoded in base36.
    JsonField(QString, clientId, {})
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
    // The mode is for forward compatibility - currently "exclude" is the only
    // supported mode.  Rules with any other mode are ignored.
    JsonField(QString, mode,QStringLiteral("exclude"))
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
    JsonField(bool, blockIPv6, true) // block IPv6 traffic
    JsonField(DNSSetting, overrideDNS, QStringLiteral("pia"), validateDNSSetting) // use PIA DNS servers (symbolic name, array with 1-2 IPs, or empty string to use existing DNS)
    JsonField(bool, allowLAN, true) // permits LAN traffic when connected/killswitched
    JsonField(bool, portForward, false) // forward a port through the VPN tunnel, see DaemonState::forwardedPort
    JsonField(bool, enableMACE, false) // Enable MACE Ad tracker
    JsonField(uint, remotePortUDP, 0) // 0 == auto
    JsonField(uint, remotePortTCP, 0) // 0 == auto
    JsonField(uint, localPort, 0) // 0 == auto
    JsonField(uint, mtu, 0) // 0 == unspecified
    JsonField(QString, cipher, QStringLiteral("AES-128-GCM"), { "AES-128-GCM", "AES-256-GCM", "AES-128-CBC", "AES-256-CBC", "none" })
    JsonField(QString, auth, QStringLiteral("SHA1"), { "SHA1", "SHA256", "none" })
    JsonField(QString, serverCertificate, QStringLiteral("RSA-2048"), { "ECDSA-256k1", "ECDSA-256r1", "ECDSA-521", "RSA-2048", "RSA-3072", "RSA-4096", "default" })
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

    // Whether split tunnel is enabled
    JsonField(bool, splitTunnelEnabled, false)
    // Rules for excluding/including apps from VPN
    JsonField(QVector<SplitTunnelRule>, splitTunnelRules, {})

    // These settings are legacy and have been moved to client-side settings.
    // They're still present in DaemonSettings so the client can migrate them.
    JsonField(bool, connectOnLaunch, false) // Connect when first client connects
    JsonField(bool, desktopNotifications, true) // Show desktop notifications
    JsonField(QVector<QString>, favoriteLocations, {})
    JsonField(QStringList, recentLocations, {})
    JsonField(QString, themeName, QStringLiteral("dark"))
    JsonField(QVector<QString>, primaryModules, QVector<QString>::fromList({QStringLiteral("region"), QStringLiteral("ip")}))
    JsonField(QVector<QString>, secondaryModules, QVector<QString>::fromList({QStringLiteral("quickconnect"), QStringLiteral("performance"), QStringLiteral("usage"), QStringLiteral("settings"), QStringLiteral("account")}))
};

// Compare QSharedPointer<ServerLocation>s by value; used by ServiceLocations
// and ConnectionInfo
inline bool compareLocationsValue(const QSharedPointer<ServerLocation> &pFirst,
                                  const QSharedPointer<ServerLocation> &pSecond)
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
    JsonField(QSharedPointer<ServerLocation>, chosenLocation, {})
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
    JsonField(QSharedPointer<ServerLocation>, bestLocation, {})
    // The next location we would connect to if the user chooses to connect (or
    // reconnect) right now.
    //
    // Like 'chosenLocation', undefined if and only if no locations are known.
    JsonField(QSharedPointer<ServerLocation>, nextLocation, {})
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
        proxy(other.proxy());
        proxyCustom(other.proxyCustom());
        proxyShadowsocks(other.proxyShadowsocks());
        proxyShadowsocksLocationAuto(other.proxyShadowsocksLocationAuto());
        return *this;
    }

    bool operator==(const ConnectionInfo &other) const
    {
        // Compare the locations by value
        return compareLocationsValue(vpnLocation(), other.vpnLocation()) &&
            vpnLocationAuto() == other.vpnLocationAuto() &&
            proxy() == other.proxy() && proxyCustom() == other.proxyCustom() &&
            compareLocationsValue(proxyShadowsocks(), other.proxyShadowsocks()) &&
            proxyShadowsocksLocationAuto() == other.proxyShadowsocksLocationAuto();
    }

    bool operator!=(const ConnectionInfo &other) const { return !(*this == other); }

public:
    // The VPN location used for this connection.  Follows the truth table in
    // DaemonState.
    JsonField(QSharedPointer<ServerLocation>, vpnLocation, {})
    // Whether the VPN location was an automatic selection
    JsonField(bool, vpnLocationAuto, false)

    // Whether the VPN is being used as the default route for this connection.
    // (Not precisely equivalent to DaemonSettings::defaultRoute; the setting is
    // only used when split tunnel is enabled.)
    JsonField(bool, defaultRoute, true)
    // The proxy type used for this connection - same values as
    // DaemonSettings::proxy (when set)
    JsonField(QString, proxy, {})
    // If proxy is 'custom', the custom proxy hostname:port that were used
    JsonField(QString, proxyCustom, {})
    // If proxy is 'shadowsocks', the Shadowsocks location that was used
    JsonField(QSharedPointer<ServerLocation>, proxyShadowsocks, {})
    // Whether the Shadowsocks location was an automatic selection
    JsonField(bool, proxyShadowsocksLocationAuto, false)
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

    // Boolean indicating whether the user wants to be connected or not.
    JsonField(bool, vpnEnabled, false)
    // The current actual state of the VPN connection.
    JsonField(QString, connectionState, QStringLiteral("Disconnected"))
    // In any of the connecting/reconnecting states, the status of the
    // connection attempt - indicates whether there is connection trouble and/or
    // we are trying alternate transports.
    JsonField(QString, connectingStatus, QStringLiteral("Connecting"))
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
    // that we actually connected with.
    // The chosen settings are set in any Connecting state and the Connected
    // state.  The actual settings are set only in the Connected state.
    // Port values of '0' (auto) have been replaced with the actual port that
    // would be used for the current location.
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
    // [Still]Connecting        | X                | -
    // Connected                | -                | X
    // Interrupted              | X                | X
    // [Still]Reconnecting      | X                | X
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

    // Locations grouped by country and sorted by latency.
    //
    // This is provided by the daemon to ensure that the client and daemon
    // handle these in exactly the same way.  Although Daemon itself only
    // technically cares about the lowest-latency location, the entire list must
    // be sorted for display in the regions list.
    //
    // The countries are sorted by the lowest latency of any location in the
    // country (which ensures that the lowest-latency location's country is
    // first).  Ties are broken by country code.
    JsonField(QVector<CountryLocations>, groupedLocations, {})

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
    // State of the network extension - the WFP callout on Windows, the
    // network kernel extension on Mac.  See Daemon::NetExtensionState.
    // This extension is currently used for the split tunnel feature but may
    // have other functionality in the future.
    // This causes the client to try to install the driver before enabling the
    // split tunnel setting if necessary, or show warnings if the driver is not
    // installed and the setting is already enabled.
    JsonField(QString, netExtensionState, QStringLiteral("NotInstalled"))

    // We failed to configure DNS on linux
    JsonField(qint64, dnsConfigFailed, 0)
    // Flag to indicate that the last time a client exited, it was an invalid exit
    // and an message should possibly be displayed
    JsonField(bool, invalidClientExit, false)

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

    // The original interface IP before we activated the VPN
    JsonField(QString, originalInterfaceIp, {})

    // The original gateway interface before we activated the VPN
    JsonField(QString, originalInterface, {})

    // A multi-function value to indicate snooze state and
    // -1 -> Snooze not active
    // 0 -> Connection transitioning from "VPN Connected" to "VPN Disconnected" because user requested Snooze
    // >0 -> The monotonic time when the snooze will be ending. Please note this can be in the past, and will be the case when the connection
    // transitions from "VPN Disconnected" to "VPN Connected" once the snooze ends
    JsonField(qint64, snoozeEndTime, -1)

    JsonField(QJsonArray, splitTunnelSupportErrors, {})

    // On Mac/Linux, the name of the tunnel device being used.  Set during the
    // [Still](Connecting|Reconnecting) states when known, remains set while
    // connected.  Cleared in the Disconnected state.  In other states, the
    // value depends on whether we had reached this phase of the last connection
    // attempt.
    JsonField(QString, tunnelDeviceName, {})
    JsonField(QString, tunnelDeviceLocalAddress, {})
    JsonField(QString, tunnelDeviceRemoteAddress, {})

public:
    QSharedPointer<ServerLocation> getClosestServerLocation();
};


#if defined(PIA_DAEMON) || defined(UNIT_TEST)

// Read the server locations given in the data from GET /vpninfo/servers, and
// build a ServerLocations collection.
//
// ServerLocations that existed previously and still exist are cloned to
// initialize the new ServerLocations, this preserves fields like 'latency' that
// don't come from the data in GET /vpninfo/servers.  (Locations are matched by
// ID.)
//
// Locations that don't have all required components are ignored.  If no valid
// locations are found, this returns an empty ServerLocations.
COMMON_EXPORT ServerLocations updateServerLocations(const ServerLocations &existingLocations,
                                                    const QJsonObject &serversObj);

// Read the Shadowsocks server locations from the SS server list, and build an
// updated ServerLocations collection.  All locations are preserved (only the
// SS server components are changed), and non-SS data in the existing locations
// collection is preserved.
COMMON_EXPORT ServerLocations updateShadowsocksLocations(const ServerLocations &existingLocations,
                                                         const QJsonObject &shadowsocksObj);

// Build the grouped and sorted locations from the flat locations.
COMMON_EXPORT QVector<CountryLocations> buildGroupedLocations(const ServerLocations &locations);

class COMMON_EXPORT NearestLocations
{
public:
    NearestLocations(const ServerLocations &locations);

public:
    // Find the closest server location that is safe to use with 'connect auto'.
    // The safe servers are found in in the "auto_regions" server json. Note
    // that servers that do NOT appear in this list may still be connected to
    // manually.
    //
    // If the portForward parameter is true, then prefer the closest location
    // that supports port forwarding.  (Still fall back to the closest location
    // if none support PF.)
    QSharedPointer<ServerLocation> getNearestSafeVpnLocation(bool portForward) const;

    // Find the closest server location that is safe and satisfies an arbitrary
    // predicate.  Does _not_ fall back if no locations satisfy the predicate.
    // (Usually used to find a location that provides an auxiliary service, like
    // Shadowsocks.)
    template<class LocationTestFunc>
    QSharedPointer<ServerLocation> getNearestSafeServiceLocation(LocationTestFunc isAllowedLocation) const
    {
        auto itResult = std::find_if(_locations.begin(), _locations.end(),
            [&isAllowedLocation](const auto &pLocation)
            {
                return pLocation && pLocation->isSafeForAutoConnect() &&
                    isAllowedLocation(*pLocation);
            });
        if(itResult != _locations.end())
            return *itResult;
        return {};
    }

private:
    QVector<QSharedPointer<ServerLocation>> _locations;
};

// Check if a DNSSetting value is Handshake (used by VpnConnection to determine
// when to start hnsd).
COMMON_EXPORT bool isDNSHandshake(const DaemonSettings::DNSSetting &setting);

extern COMMON_EXPORT const QString hnsdLocalAddress;

// Translate the DNS override setting into an actual list of DNS servers (empty
// if DNS should not be overridden).
COMMON_EXPORT QStringList getDNSServers(const DaemonSettings::DNSSetting& setting);

#endif

#endif // SETTINGS_H
