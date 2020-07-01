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
  // Constants
  readonly property var defaultDebugLogging: NativeDaemon.settings.getDefaultDebugLogging()

  // Settings
  readonly property string lastUsedVersion: NativeDaemon.settings.lastUsedVersion
  readonly property string location: NativeDaemon.settings.location
  readonly property string infrastructure: NativeDaemon.settings.infrastructure
  readonly property bool includeGeoOnly: NativeDaemon.settings.includeGeoOnly
  readonly property string method: NativeDaemon.settings.method
  readonly property string protocol: NativeDaemon.settings.protocol
  readonly property string killswitch: NativeDaemon.settings.killswitch
  readonly property bool defaultRoute: NativeDaemon.settings.defaultRoute
  readonly property bool blockIPv6: NativeDaemon.settings.blockIPv6
  readonly property var overrideDNS: NativeDaemon.settings.overrideDNS
  readonly property bool allowLAN: NativeDaemon.settings.allowLAN
  readonly property bool portForward: NativeDaemon.settings.portForward
  readonly property bool enableMACE: NativeDaemon.settings.enableMACE
  readonly property int remotePortUDP: NativeDaemon.settings.remotePortUDP
  readonly property int remotePortTCP: NativeDaemon.settings.remotePortTCP
  readonly property int localPort: NativeDaemon.settings.localPort
  readonly property int mtu: NativeDaemon.settings.mtu
  readonly property string cipher: NativeDaemon.settings.cipher
  readonly property string auth: NativeDaemon.settings.auth
  readonly property string serverCertificate: NativeDaemon.settings.serverCertificate
  readonly property string windowsIpMethod: NativeDaemon.settings.windowsIpMethod
  readonly property string proxy: NativeDaemon.settings.proxy
  readonly property var proxyCustom: NativeDaemon.settings.proxyCustom
  readonly property string proxyShadowsocksLocation: NativeDaemon.settings.proxyShadowsocksLocation
  readonly property bool automaticTransport: NativeDaemon.settings.automaticTransport
  readonly property var debugLogging: NativeDaemon.settings.debugLogging
  readonly property string updateChannel: NativeDaemon.settings.updateChannel
  readonly property string betaUpdateChannel: NativeDaemon.settings.betaUpdateChannel
  readonly property bool offerBetaUpdates: NativeDaemon.settings.offerBetaUpdates
  readonly property bool splitTunnelEnabled: NativeDaemon.settings.splitTunnelEnabled
  readonly property var splitTunnelRules: NativeDaemon.settings.splitTunnelRules
  readonly property var bypassSubnets: NativeDaemon.settings.bypassSubnets
  readonly property bool wireguardUseKernel: NativeDaemon.settings.wireguardUseKernel
  readonly property int wireguardPingTimeout: NativeDaemon.settings.wireguardPingTimeout
  readonly property bool persistDaemon: NativeDaemon.settings.persistDaemon
  readonly property string macStubDnsMethod: NativeDaemon.settings.macStubDnsMethod

  // Deprecated settings aren't passed through to QML, they're migrated by
  // ClientInterface
}
