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

#pragma once
#include <common/src/common.h>
#include <common/src/json.h>
#include <common/src/vpnstate.h>
#include <common/src/jsonstate.h>
#include "clientjson.h"
#include <common/src/settings/locations.h>
#include <common/src/settings/connection.h>
#include <common/src/settings/automation.h>
#include <nlohmann/json.hpp>

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
class ConnectionInfo
{
public:
    ConnectionInfo() = default;
    ConnectionInfo(QSharedPointer<const Location> pVpnLocation,
        bool vpnLocationAuto, QString method, bool methodForcedByAuth,
        QString dnsType, QString openvpnCipher, bool otherAppsUseVpn,
        QString proxy, QString proxyCustom,
        QSharedPointer<const Location> pProxyShadowsocks,
        bool proxyShadowsocksLocationAuto, bool portForward);

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
            proxy() == other.proxy() &&
            proxyCustom() == other.proxyCustom() &&
            compareLocationsValue(proxyShadowsocks(), other.proxyShadowsocks()) &&
            proxyShadowsocksLocationAuto() == other.proxyShadowsocksLocationAuto() &&
            portForward() == other.portForward();
    }

    bool operator!=(const ConnectionInfo &other) const { return !(*this == other); }

public:
    // The VPN location used for this connection.  Follows the truth table in
    // DaemonState.
    const QSharedPointer<const Location> &vpnLocation() const {return _pVpnLocation;}
    // Whether the VPN location was an automatic selection
    bool vpnLocationAuto() const {return _vpnLocationAuto;}

    // The VPN method used for this connection
    QString method() const {return _method;}
    // Whether the VPN method was forced to OpenVPN due to lack of an auth token
    bool methodForcedByAuth() const {return _methodForcedByAuth;}

    // DNS type used for this connection
    QString dnsType() const {return _dnsType;}

    // OpenVPN cryptographic settings - only meaningful when method is OpenVPN
    QString openvpnCipher() const {return _openvpnCipher;}

    // Whether the default app behavior is to use the VPN - i.e. whether apps
    // with no specific rules use the VPN (always true when split tunnel is
    // disabled).
    bool otherAppsUseVpn() const {return _otherAppsUseVpn;}
    // The proxy type used for this connection - same values as
    // DaemonSettings::proxyType when enabled, or "none" when disabled
    // (historical value of combined `DaemonSettings::proxy` setting for no
    // proxy)
    QString proxy() const {return _proxy;}
    // If proxy is 'custom', the custom proxy hostname:port that were used
    QString proxyCustom() const {return _proxyCustom;}
    // If proxy is 'shadowsocks', the Shadowsocks location that was used
    const QSharedPointer<const Location> &proxyShadowsocks() const {return _pProxyShadowsocks;}
    // Whether the Shadowsocks location was an automatic selection
    bool proxyShadowsocksLocationAuto() const {return _proxyShadowsocksLocationAuto;}

    // Whether port forwarding is enabled for this connection
    bool portForward() const {return _portForward;}

private:
    QSharedPointer<const Location> _pVpnLocation{};
    bool _vpnLocationAuto{false};
    QString _method{QStringLiteral("openvpn")}; // "openvpn", "wireguard"
    bool _methodForcedByAuth{false};
    QString _dnsType{QStringLiteral("pia")};    // "pia", "handshake", "local", "existing", "custom"
    QString _openvpnCipher{};
    bool _otherAppsUseVpn{true};
    QString _proxy{};
    QString _proxyCustom{};
    QSharedPointer<const Location> _pProxyShadowsocks{};
    bool _proxyShadowsocksLocationAuto{false};
    bool _portForward{false};
};

namespace clientjson
{
    template<>
    struct serializer<ConnectionInfo>
    {
        static void to_json(json &j, const ConnectionInfo &ci)
        {
            j = {
                {"vpnLocation", ci.vpnLocation()},
                {"vpnLocationAuto", ci.vpnLocationAuto()},
                {"method", ci.method()},
                {"methodForcedByAuth", ci.methodForcedByAuth()},
                {"dnsType", ci.dnsType()},
                {"openvpnCipher", ci.openvpnCipher()},
                {"otherAppsUseVpn", ci.otherAppsUseVpn()},
                {"proxy", ci.proxy()},
                {"proxyCustom", ci.proxyCustom()},
                {"proxyShadowsocks", ci.proxyShadowsocks()},
                {"proxyShadowsocksLocationAuto", ci.proxyShadowsocksLocationAuto()},
                {"portForward", ci.portForward()}
            };
        }
    };
}

// This is the Daemon's model of its own state expressed to clients.  The
// internal model has strong invariants describing the daemon state, and can be
// serialized to JSON (but not from JSON, as this isn't needed for DaemonState
// and would need to handle all invariants)
//
// DaemonState includes information describing the daemon's current state, which
// is not persisted - it is reconstructed from the persisted settings and data
// models.
class StateModel : public JsonState<clientjson::json>
{
    Q_GADGET    // for Q_ENUM(NetExtensionState)

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

public:
    // Let the client know whether we currently have an auth token; the client
    // uses this to detect the "logged in, but API unreachable" state (where we
    // will try to connect to the VPN server using credential auth).  The client
    // can't access the actual auth token.
    JsonProperty(bool, hasAccountToken);

    // Boolean indicating whether the user wants to be connected or not.
    // This specifically tracks the user's intent - this should _only_ ever be
    // changed due to a user request to connect or disconnect.
    //
    // In general, any connection state can occur for any value of vpnEnabled.
    // It is even possible to be "Disconnected" while "vpnEnabled == true"; this
    // happens if a fatal error causes a reconnection to abort.  In this case
    // we correctly have vpnEnabled=true, because the user intended to be
    // connected, but the app cannot try to connect due to the fatal error.
    JsonProperty(bool, vpnEnabled);
    // The current actual state of the VPN connection.
    JsonProperty(QString, connectionState, QStringLiteral("Disconnected"));
    // When in a connecting state, enabled when enough attempts have been made
    // to trigger the 'slow' attempt intervals.  Resets to false before going
    // to a non-connecting state, or when settings change during a series of
    // attempts.
    JsonProperty(bool, usingSlowInterval);
    // Boolean indicating whether a reconnect is needed in order to apply settings changes.
    JsonProperty(bool, needsReconnect);
    // Total number of bytes received over the VPN.
    JsonProperty(uint64_t, bytesReceived);
    // Total number of bytes sent over the VPN.
    JsonProperty(uint64_t, bytesSent);
    // When DaemonSettings::portForward has been enabled, the port that was
    // forwarded.  Positive values are a forwarded port; other values are
    // special values from PortForwardState.
    JsonProperty(int, forwardedPort, VpnState::PortForwardState::Inactive);
    // External non-VPN IP address detected before connecting to the VPN
    JsonProperty(QString, externalIp);
    // External VPN IP address detected after connecting
    JsonProperty(QString, externalVpnIp);

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
    JsonProperty(Optional<Transport>, chosenTransport);
    JsonProperty(Optional<Transport>, actualTransport);

    // Service locations chosen by the daemon, based on the chosen and best
    // locations, etc.
    //
    // All location choices are provided for the VPN service and for the
    // Shadowsocks service.  The logic for determining each one is different
    // ("auto" means different things, for example), but the meaning of each
    // field is the same ("the next location we would use", "the location we
    // would use for auto", etc.)
    JsonProperty(ServiceLocations, vpnLocations);
    JsonProperty(ServiceLocations, shadowsocksLocations);

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
    JsonProperty(ConnectionInfo, connectingConfig);
    JsonProperty(ConnectionInfo, connectedConfig);
    // The next configuration we would use to connect if we connected right now.
    // This is set based on the current settings, regions list, etc.
    JsonProperty(ConnectionInfo, nextConfig);

    // The specific server that we have connected to, when connected (only
    // provided in the Connected state)
    JsonProperty(Optional<Server>, connectedServer);

    // Available regions, mapped by region ID.  These are from either the
    // current or new regions list.  This includes both dedicated IP regions and
    // regular regions, which are treated the same way by most logic referring
    // to regions.
    JsonProperty(LocationsById, availableLocations);

    // Metadata for all locations and countries; includes map coordinates,
    // display texts, etc.
    JsonProperty(kapps::regions::Metadata, regionsMetadata);

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
    JsonProperty(std::vector<CountryLocations>, groupedLocations);

    // Dedicated IP locations sorted by latency with the same tie-breaking logic
    // as groupedLocations().  This is used in display contexts alongside
    // groupedLocations(), as dedicated IP regions are displayed differently.
    JsonProperty(std::vector<QSharedPointer<const Location>>, dedicatedIpLocations);

    // All supported ports for the OpenVpnUdp and OpenVpnTcp services in the
    // active infrastructure (union of the supported ports among all advertised
    // servers).  This can be derived from the regions lists above, but this
    // derivation is relatively complex so these are stored.
    //
    // This is just used to define the choices presented in the "Remote Port"
    // drop-down.
    JsonProperty(DescendingPortSet, openvpnUdpPortChoices);
    JsonProperty(DescendingPortSet, openvpnTcpPortChoices);

    // Per-interval bandwidth measurements while connected to the VPN.  Only a
    // limited number of intervals are kept (new values past the limit will bump
    // off the oldest value).  Older values are first.
    //
    // When not connected, this is an empty array.
    JsonProperty(std::deque<IntervalBandwidth>, intervalMeasurements);
    // Timestamp when the VPN connection was established - ms since system
    // startup, using a monotonic clock.  0 if we are not connected.
    //
    // Monotonic time is used so that changes in the wall-clock time won't
    // affect the computed duration.  However, monotonic time usually excludes
    // time while the system is sleeping/hibernating.  Most of the time, this
    // will force us to reconnect anyway, but if the system sleeps for a short
    // enough time that the connection is still alive, it is not too surprising
    // that the connection duration would exclude the sleep time.
    JsonProperty(qint64, connectionTimestamp);

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
    JsonProperty(std::deque<QString>, overridesFailed);
    // Testing override(s) are active.  Human-readable names of the overridden
    // features; set at daemon startup, like overridesFailed.
    JsonProperty(std::deque<QString>, overridesActive);
    // Authorization failed in the OpenVPN connection (timestamp of failure).
    // Note that this does not really mean that the user's credentials are
    // incorrect, see ClientNotifications.qml.
    JsonProperty(qint64, openVpnAuthFailed);
    // Connection was lost (timestamp)
    JsonProperty(qint64, connectionLost);
    // Failed to resolve the configured proxy.
    JsonProperty(qint64, proxyUnreachable);
    // Killswitch rules blocking Internet access are active.  Note that this can
    // apply in the Connecting/Connected states too, but usually shouldn't be
    // displayed in these states.
    JsonProperty(bool, killswitchEnabled);
    // Available update version - set when the newest version advertised on the
    // active release channel(s) is different from the daemon version; empty if
    // no update is available or it is the same version as the daemon.  The
    // client offers to download this version when it's set.
    // Note that the download URI is not provided since it is not used by the
    // client.
    JsonProperty(QString, availableVersion);
    // Enabled if the current OS is out of support - newer updates are available
    // but they do not support this OS version.
    JsonProperty(bool, osUnsupported);
    // When a download has been initiated, updateDownloadProgress indicates the
    // progress (as a percentage).  -1 means no download is occurring,
    // 0-100 indicates that a download is ongoing.  When the download completes,
    // updateInstallerPath is set.
    JsonProperty(int, updateDownloadProgress, -1);
    // The path to the installer for an update that has been downloaded.  Empty
    // if no installer has been downloaded.
    JsonProperty(QString, updateInstallerPath);
    // If a download attempt fails, updateDownloadFailure is set to the
    // timestamp of the failure.  This is cleared when a new download is
    // attempted.
    JsonProperty(qint64, updateDownloadFailure);
    // The version of the installer downloaded (when updateInstallerPath is
    // set), being downloaded (when updateDownloadProgress is set), or that
    // failed (when updateDownloadFailure is set)
    JsonProperty(QString, updateVersion);
    // The TAP adapter is missing on Windows (the client offers to reinstall it)
    // Not dismissible, so this is just a boolean flag.
    JsonProperty(bool, tapAdapterMissing);
    // The WinTUN driver is missing on Windows.  Like the TAP error, the client
    // offers to reinstall it, and this is not dismissible.
    JsonProperty(bool, wintunMissing);
    // State of the network extension - the WFP callout on Windows, the
    // network kernel extension on Mac.  See Daemon::NetExtensionState.
    // This extension is currently used for the split tunnel feature but may
    // have other functionality in the future.
    // This causes the client to try to install the driver before enabling the
    // split tunnel setting if necessary, or show warnings if the driver is not
    // installed and the setting is already enabled.
    JsonProperty(QString, netExtensionState, QStringLiteral("NotInstalled"));
    // Result of the connection test performed after connecting to the VPN.  If
    // the connection is not working, this will be set, and the client will show
    // a warning.
    JsonProperty(bool, connectionProblem);
    // A dedicated IP will expire soon.  When active, the number of days until
    // the next expiration is also given.
    JsonProperty(quint64, dedicatedIpExpiring);
    JsonProperty(int, dedicatedIpDaysRemaining);
    // A dedicated IP has changed (as observed by the daemon when refreshing
    // DIP info).  Cleared if the notification is dismissed.
    JsonProperty(quint64, dedicatedIpChanged);

    // We failed to configure DNS on linux
    JsonProperty(qint64, dnsConfigFailed);
    // Flag to indicate that the last time a client exited, it was an invalid exit
    // and an message should possibly be displayed
    JsonProperty(bool, invalidClientExit);
    // Flag to indicate that the daemon killed the last client connection.
    // Similar to invalidClientExit, but does not trigger any client warning,
    // since this is normally caused by the OS freezing the client process, and
    // we expect the client process to reconnect.
    JsonProperty(bool, killedClient);

    // hnsd is failing to launch.  Set after it fails for 10 seconds, cleared
    // when it launches successfully and runs for at least 30 seconds.
    // (Timestamp of first warning.)
    JsonProperty(qint64, hnsdFailing);
    // hnsd is failing to sync (but it is running, or at least it was at some
    // point).  Set if it runs for 5 seconds without syncing a block, cleared
    // once it syncs a block.  This can overlap with hnsdFailing if it also
    // crashes or restarts after this condition occurs.
    JsonProperty(qint64, hnsdSyncFailure);

    // The original gateway IP address before we activated the VPN
    JsonProperty(QString, originalGatewayIp);

    // The original interface IP and network prefix before we activated the VPN
    JsonProperty(QString, originalInterfaceIp);
    JsonProperty(unsigned, originalInterfaceNetPrefix);
    JsonProperty(unsigned, originalMtu);

    // The original gateway interface before we activated the VPN
    JsonProperty(QString, originalInterface);

    // The original IPv6 interface IP, gateway, and MTU before we activated the VPN
    JsonProperty(QString, originalInterfaceIp6);
    JsonProperty(QString, originalGatewayIp6);
    JsonProperty(unsigned, originalMtu6);

    // The key for the primary service on macOS
    JsonProperty(QString, macosPrimaryServiceKey);

    // A multi-function value to indicate snooze state and
    // -1 -> Snooze not active
    // 0 -> Connection transitioning from "VPN Connected" to "VPN Disconnected" because user requested Snooze
    // >0 -> The monotonic time when the snooze will be ending. Please note this can be in the past, and will be the case when the connection
    // transitions from "VPN Disconnected" to "VPN Connected" once the snooze ends
    JsonProperty(qint64, snoozeEndTime, -1);

    // If split tunnel is not available, this is set to a list of reasons.
    // The reasons are listed in SettingsMessages.qml along with their UI text.
    // (For example - "libnl_invalid" is set if the libnl libraries can't be
    // loaded on Linux.)
    JsonProperty(std::vector<QString>, splitTunnelSupportErrors);

    // A key component for the VPN is not available, all connections must be 
    // prevented the user should be warned and the daemon should not start in 
    // this state. 
    JsonProperty(std::vector<QString>, vpnSupportErrors);

    // On Mac/Linux, the name of the tunnel device being used.  Set during the
    // [Still](Connecting|Reconnecting) states when known, remains set while
    // connected.  Cleared in the Disconnected state.  In other states, the
    // value depends on whether we had reached this phase of the last connection
    // attempt.
    JsonProperty(QString, tunnelDeviceName);
    JsonProperty(QString, tunnelDeviceLocalAddress);
    JsonProperty(QString, tunnelDeviceRemoteAddress);

    // Whether WireGuard is available at all on this OS.  (False on Windows 7.)
    JsonProperty(bool, wireguardAvailable, true);
    // Whether a kernel implementation of Wireguard is available (only possible
    // on Linux).
    JsonProperty(bool, wireguardKernelSupport);
    // The DNS servers prior to connecting
    JsonProperty(std::vector<quint32>, existingDNSServers);

    // Automation rules - indicates which rule has triggered, which rule
    // currently matches, the rule that could be created for the current
    // network, etc.

    // If automation rules aren't available, this is set to a list of reasons.
    // The possible reasons are a subset of those used for
    // splitTunnelSupportErrors; the same text from SettingsMessages is used in
    // the UI.
    JsonProperty(std::vector<QString>, automationSupportErrors);

    // The rule that caused the last VPN connection transition, if there is one.
    // If the last transition was manual instead (connect button, snooze, etc.),
    // this is 'null'.
    //
    // This is a complete Rule with both action and condition
    JsonProperty(Optional<AutomationRule>, automationLastTrigger);

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
    JsonProperty(Optional<AutomationRule>, automationCurrentMatch);

    // These are rule conditions for wireless networks that are currently
    // connected.  Note that there can in principle be more than one of these
    // (if the device has multiple Wi-Fi interfaces), and these may not be the
    // default network (if, say, Ethernet is also connected).
    //
    // Conditions are populated for each connected wireless network, even if
    // rules already exist for them.
    JsonProperty(std::vector<AutomationRuleCondition>, automationCurrentNetworks);
};
