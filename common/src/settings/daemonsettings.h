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

#ifndef SETTINGS_DAEMONSETTINGS_H
#define SETTINGS_DAEMONSETTINGS_H

#include "common.h"
#include "json.h"
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
    JsonField(int, mtu, -1) // 0 == unspecified, large; -1 == Path MTU detection
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


#endif
