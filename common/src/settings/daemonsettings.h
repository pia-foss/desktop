// Copyright (c) 2026 Private Internet Access, Inc.
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

#ifndef SETTINGS_DAEMONSETTINGS_H
#define SETTINGS_DAEMONSETTINGS_H

#include "../common.h"
#include "../json.h"
#include <unordered_set>
#include <vector>
#include <QVector>
#include "splittunnel.h"
#include "automation.h"
#include "connection.h"

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
    JsonField(QString, method, QStringLiteral("wireguard"), { "openvpn", "wireguard" })
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
    // Remote ports used for OpenVPN connections to a dedicated IP.  These are
    // separate from remotePortUDP/TCP because DIP over OpenVPN targets
    // DIP-specific ports.  0, or a port the DIP doesn't offer, uses the DIP's
    // first port.
    JsonField(uint, dipRemotePortUDP, 0)
    JsonField(uint, dipRemotePortTCP, 0)
    JsonField(int, mtu, -1) // 0 == unspecified, large; -1 == Path MTU detection
    JsonField(QString, cipher, QStringLiteral("AES-128-GCM"), { "AES-128-GCM", "AES-256-GCM" })
    // On Windows, the driver and configuration method to use to configure the
    // virtual network interface.  "wintun" is the default.
    //
    // Historically, this only controlled the IP configuration method for the
    // TAP adapter, but WinTUN was added as an alternative in 2.11 since we
    // only have one configuration method for WinTUN, and DCO was added later.
    // The name is kept for compatibility with existing settings files.
    //
    // - dco - use the OpenVPN DCO driver, which offloads the data channel to
    //   the kernel.  DCO only supports UDP, and it cannot do compression stub
    //   framing; WinTUN is used automatically when DCO can't carry the
    //   connection (see OpenVPNMethod::run()).
    // - wintun - use the WinTUN driver and OpenVPN's built-in static
    //   configuration method.
    // - dhcp - use the TAP driver and "ip-win32 dhcp", IP configuration and
    //   DNS servers are applied using a phony DHCP negotiation with the TAP
    //   driver acting as a DHCP server.
    //   This method has historically been the most compatible for TAP, but it
    //   is known to fail on some systems when the DHCP negotiation fails or
    //   times out.
    // - static - use the TAP driver and "ip-win32 netsh".  DNS servers are
    //   applied using our updown script, which uses netsh.  This avoids DHCP
    //   timeouts, but netsh is prone to spurious failures ("The parameter is
    //   incorrect"), and it depends on a running DNS Client service, which
    //   users sometimes disable.
    JsonField(QString, windowsIpMethod, QStringLiteral("wintun"), {"dco", "wintun", "dhcp", "static"})

    // Proxy setting
    //  - "custom" - Use proxyCustom
    //  - "shadowsocks" - Use a PIA shadowsocks region - proxyShadowsocksLocation
    // May have additional values in the future, such as using a PIA region.
    JsonField(QString, proxyType, QStringLiteral("shadowsocks"), {"custom", "shadowsocks"})

    // Whether a proxy is enabled, used in conjunction with "proxyType"
    JsonField(bool, proxyEnabled, false)

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

    // Store the version where the user last opted in for service quality
    // events. Storing the version instead of a boolean field enables
    // the option to conditionally include or remove metrics depending on
    // the version.
    JsonField(QString, serviceQualityAcceptanceVersion, "")

    // Persists the split tunnel activation-failed banner across daemon restarts.
    // Cleared when the user dismisses the notification or successfully re-enables split tunnel.
    JsonField(bool, splitTunnelActivationFailed, false)

    // Whether split tunnel is enabled
    JsonField(bool, splitTunnelEnabled, false)
    // Whether to also split DNS traffic - set to false by default, as it can be a little
    // temperamental on some platforms
    JsonField(bool, splitTunnelDNS, false)
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

    // Whether to obfuscate WireGuard with Amnezia.  When true, only servers
    // with obfuscation are used; when false, only vanilla WireGuard servers.
    // Not supported on Windows - always treated as false there.
    JsonField(bool, wireguardObfuscation, false)

    // If no data is received for (wireguardPingTimeout/2) seconds, fire off a ping.
    // If no data is recieved for another (wireguardPingTimeout/2) seconds, assume that the connection
    // is lost
    //
    // Should be a multiple of statsInterval (5)
    JsonField(uint, wireguardPingTimeout, 30)

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

    // Store the number of attempted sessions used for triggering rating request
    JsonField(int, sessionCount, 0)

    // Store a count of successful sessions - used for triggering survey request
    JsonField(int, successfulSessionCount, 0)

    // Set to false when user opts out, or finishes rating successfully.
    JsonField(bool, ratingEnabled, true)

    // Set to false when user opts out, or finishes rating successfully.
    JsonField(bool, surveyRequestEnabled, true)

    // Keep track of the id of the last dismissed in-app message
    JsonField(quint64, lastDismissedAppMessageId, 0)

    // Keep extra logs on disk for debugging/diagnosis purposes. This is disabled
    // by default and can only be turned on using the CLI
    JsonField(bool, largeLogFiles, false)

    // Whether to allow server latency to be calculated in the background
    // when the VPN is disconnected
    JsonField(bool, enableBackgroundLatencyChecks, true)

    // Whether to show in-app communication messages to the user
    JsonField(bool, showAppMessages, true)

    // Manual server dev setting - for testing specific servers
    JsonField(ManualServer, manualServer, {})

    // Whether the VPN should connect on wake after a sleep - currently only applies to macOS.
    // This feature exists to work-around a bug where if the VPN goes to sleep and is
    // still connected it can burn through file descriptors constantly trying to reconnect.
    // So our approach is to disconnect the VPN when asleep (if it was previously connected),
    // engage killswitch and then reconnect immediately upon waking.
    JsonField(bool, connectOnWake, false);
};


#endif
