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

import QtQuick 2.0
import PIA.NativeDaemon 1.0
import PIA.NativeHelpers 1.0
import "../client"

QtObject {
  readonly property bool hasAccountToken: NativeDaemon.state.hasAccountToken
  readonly property bool vpnEnabled: NativeDaemon.state.vpnEnabled
  readonly property string connectionState: NativeDaemon.state.connectionState
  readonly property int usingSlowInterval: NativeDaemon.state.usingSlowInterval
  readonly property bool needsReconnect: NativeDaemon.state.needsReconnect
  readonly property double bytesReceived: NativeDaemon.state.bytesReceived
  readonly property double bytesSent: NativeDaemon.state.bytesSent
  readonly property int forwardedPort: NativeDaemon.state.forwardedPort
  readonly property string externalIp: NativeDaemon.state.externalIp
  readonly property string externalVpnIp: NativeDaemon.state.externalVpnIp
  readonly property var chosenTransport: NativeDaemon.state.chosenTransport
  readonly property var actualTransport: NativeDaemon.state.actualTransport
  readonly property var vpnLocations: NativeDaemon.state.vpnLocations
  readonly property var shadowsocksLocations: NativeDaemon.state.shadowsocksLocations
  readonly property var connectingConfig: NativeDaemon.state.connectingConfig
  readonly property var connectedConfig: NativeDaemon.state.connectedConfig
  readonly property var nextConfig: NativeDaemon.state.nextConfig
  readonly property var connectedServer: NativeDaemon.state.connectedServer
  readonly property var availableLocations: NativeDaemon.state.availableLocations
  readonly property var groupedLocations: NativeDaemon.state.groupedLocations
  readonly property var dedicatedIpLocations: NativeDaemon.state.dedicatedIpLocations
  readonly property var openvpnUdpPortChoices: NativeDaemon.state.openvpnUdpPortChoices
  readonly property var openvpnTcpPortChoices: NativeDaemon.state.openvpnTcpPortChoices
  readonly property var intervalMeasurements: NativeDaemon.state.intervalMeasurements
  readonly property double connectionTimestamp: NativeDaemon.state.connectionTimestamp
  readonly property var overridesFailed: NativeDaemon.state.overridesFailed
  readonly property var overridesActive: NativeDaemon.state.overridesActive
  readonly property double openVpnAuthFailed: NativeDaemon.state.openVpnAuthFailed
  readonly property double connectionLost: NativeDaemon.state.connectionLost
  readonly property double proxyUnreachable: NativeDaemon.state.proxyUnreachable
  readonly property bool killswitchEnabled: NativeDaemon.state.killswitchEnabled
  readonly property string availableVersion: NativeDaemon.state.availableVersion
  readonly property bool osUnsupported: NativeDaemon.state.osUnsupported
  readonly property int updateDownloadProgress: NativeDaemon.state.updateDownloadProgress
  readonly property string updateInstallerPath: NativeDaemon.state.updateInstallerPath
  readonly property double updateDownloadFailure: NativeDaemon.state.updateDownloadFailure
  readonly property string updateVersion: NativeDaemon.state.updateVersion
  readonly property double dnsConfigFailed: NativeDaemon.state.dnsConfigFailed
  readonly property bool tapAdapterMissing: NativeDaemon.state.tapAdapterMissing
  readonly property bool wintunMissing: NativeDaemon.state.wintunMissing
  readonly property string netExtensionState: NativeDaemon.state.netExtensionState
  readonly property bool connectionProblem: NativeDaemon.state.connectionProblem
  readonly property double dedicatedIpExpiring: NativeDaemon.state.dedicatedIpExpiring
  readonly property int dedicatedIpDaysRemaining: NativeDaemon.state.dedicatedIpDaysRemaining
  readonly property double dedicatedIpChanged: NativeDaemon.state.dedicatedIpChanged
  readonly property bool invalidClientExit: NativeDaemon.state.invalidClientExit
  readonly property bool killedClient: NativeDaemon.state.killedClient
  readonly property double hnsdFailing: NativeDaemon.state.hnsdFailing
  readonly property double hnsdSyncFailure: NativeDaemon.state.hnsdSyncFailure
  readonly property string originalGatewayIp: NativeDaemon.state.originalGatewayIp
  readonly property string originalInterfaceIp: NativeDaemon.state.originalInterfaceIp
  readonly property string originalInterface: NativeDaemon.state.originalInterface
  readonly property double snoozeEndTime: NativeDaemon.state.snoozeEndTime
  readonly property var splitTunnelSupportErrors: NativeDaemon.state.splitTunnelSupportErrors
  readonly property string tunnelDeviceName: NativeDaemon.state.tunnelDeviceName
  readonly property string tunnelDeviceLocalAddress: NativeDaemon.state.tunnelDeviceLocalAddress
  readonly property string tunnelDeviceRemoteAddress: NativeDaemon.state.tunnelDeviceRemoteAddress
  readonly property bool wireguardAvailable: NativeDaemon.state.wireguardAvailable
  readonly property bool wireguardKernelSupport: NativeDaemon.state.wireguardKernelSupport
  readonly property bool existingDNSServers: NativeDaemon.state.existingDNSServers
  readonly property var automationLastTrigger: NativeDaemon.state.automationLastTrigger
  readonly property var automationCurrentMatch: NativeDaemon.state.automationCurrentMatch
  readonly property var automationCurrentNetworks: NativeDaemon.state.automationCurrentNetworks

  // Constants for special values of forwardedPort (see PortForwarder::Special)
  readonly property var portForward: {
    'inactive': 0,
    'attempting': -1,
    'failed': -2,
    'unavailable': -3
  }

  // Location geographic coordinates - from region metadata
  readonly property var locationCoords: Daemon.data.modernRegionMeta.gps || {}

  // The regions list just gives us country codes, not names.  Map the codes to
  // names here using region metadata.
  readonly property var countryNames: Daemon.data.modernRegionMeta['country_groups'] || {}

  function translateName (name) {
    // Add a dependency on uiTr() so this retranslates at the right time
    let trDep = uiTr

    let translations = Daemon.data.modernRegionMeta['translations'] || {};

    if(translations.hasOwnProperty(name)) {
      if(translations[name].hasOwnProperty(Client.settings.language)) {
        return translations[name][Client.settings.language];
      }
    }

    return name;
  }

  function getBestLocationForCountry(countryCode) {
    return NativeHelpers.getBestLocationForCountry(NativeDaemon.state, countryCode)
  }
}
