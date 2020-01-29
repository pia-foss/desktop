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

import QtQuick 2.0
import PIA.NativeDaemon 1.0

QtObject {
  readonly property bool vpnEnabled: NativeDaemon.state.vpnEnabled
  readonly property string connectionState: NativeDaemon.state.connectionState
  readonly property string connectingStatus: NativeDaemon.state.connectingStatus
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
  readonly property var groupedLocations: NativeDaemon.state.groupedLocations
  readonly property var intervalMeasurements: NativeDaemon.state.intervalMeasurements
  readonly property double connectionTimestamp: NativeDaemon.state.connectionTimestamp
  readonly property double openVpnAuthFailed: NativeDaemon.state.openVpnAuthFailed
  readonly property double connectionLost: NativeDaemon.state.connectionLost
  readonly property double proxyUnreachable: NativeDaemon.state.proxyUnreachable
  readonly property bool killswitchEnabled: NativeDaemon.state.killswitchEnabled
  readonly property string availableVersion: NativeDaemon.state.availableVersion
  readonly property int updateDownloadProgress: NativeDaemon.state.updateDownloadProgress
  readonly property string updateInstallerPath: NativeDaemon.state.updateInstallerPath
  readonly property double updateDownloadFailure: NativeDaemon.state.updateDownloadFailure
  readonly property string updateVersion: NativeDaemon.state.updateVersion
  readonly property double dnsConfigFailed: NativeDaemon.state.dnsConfigFailed
  readonly property bool tapAdapterMissing: NativeDaemon.state.tapAdapterMissing
  readonly property string netExtensionState: NativeDaemon.state.netExtensionState
  readonly property bool invalidClientExit: NativeDaemon.state.invalidClientExit
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

  // Constants for special values of forwardedPort (see PortForwarder::Special)
  readonly property var portForward: {
    'inactive': 0,
    'attempting': -1,
    'failed': -2,
    'unavailable': -3
  }
}
