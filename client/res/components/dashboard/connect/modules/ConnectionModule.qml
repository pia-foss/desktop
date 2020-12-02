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

import QtQuick 2.9
import QtQuick.Controls 2.3
import "../../../common"
import "../../../core"
import "../../../daemon"
import "../../../theme"
import "../../../settings"
import "../../../settings/stores"
import PIA.NativeAcc 1.0 as NativeAcc
import PIA.NativeHelpers 1.0
import QtQuick.Layouts 1.3

MovableModule {
  implicitHeight: 120
  moduleKey: 'connection'

  //: Screen reader annotation for the Connection tile
  tileName: uiTr("Connection tile")
  NativeAcc.Group.name: tileName

  property string mouseOverMetric: ""


  Text {
    text: mouseOverMetric || uiTr("CONNECTION")
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 10
    width: 260
    elide: Text.ElideRight
  }

  readonly property bool showWireguard: Daemon.displayConnectionConfig.method === 'wireguard'


  GridLayout {
    x: 20
    y: 32
    height: 70
    width: 260

    columns: 2
    rowSpacing: 0
    columnSpacing: 0

    ConnectionModuleMetric {
      // The protocol can be either OpenVPN or WireGuard.  This should match the
      // label used on the Connection page of Settings, but without the trailing
      // colon.
      metricName: uiTr("Protocol")
      iconPath: Theme.dashboard.connectionModuleConnectionImage
      metricValue: showWireguard ? "WireGuard®" : "OpenVPN®"
      topRow: true
    }
    ConnectionModuleMetric {
      metricName: SettingsMessages.connectionTypeSetting
      iconPath: Theme.dashboard.connectionModuleSocketImage
      metricValue: {
        if(showWireguard)
          return "UDP"
        var proto = ""
        // When connected, show the actual transport used, otherwise show the
        // setting
        if(Daemon.state.connectionState === "Connected" && Daemon.state.actualTransport)
          proto = Daemon.state.actualTransport.protocol
        else
          proto = Daemon.settings.protocol
        if(proto === "udp")
          return "UDP"
        return "TCP"
      }
      topRow: true
    }
    ConnectionModuleMetric {
      metricName: SettingsMessages.remotePortSetting
      iconPath: Theme.dashboard.connectionModulePortImage
      metricValue: {
        // WireGuard always uses 1337 currently
        if(showWireguard)
          return "1337"
        // When connected, show the actual transport used
        if(Daemon.state.connectionState === "Connected" && Daemon.state.actualTransport)
          return Daemon.state.actualTransport.port.toString()
        // Otherwise, show the setting value
        var port = 0
        if(Daemon.settings.protocol === "udp")
          port = Daemon.settings.remotePortUDP
        else
          port = Daemon.settings.remotePortTCP
        if(port === 0)
          return SettingsMessages.defaultRemotePort
        return port.toString()
      }
    }
    ConnectionModuleMetric {
      metricName: SettingsMessages.dataEncryptionSetting
      iconPath: Theme.dashboard.connectionModuleEncryptionImage
      metricValue: {
        if(showWireguard)
          return "ChaCha20"
        return Daemon.displayConnectionConfig.openvpnCipher
      }
    }
    ConnectionModuleMetric {
      metricName: SettingsMessages.dataAuthenticationSetting
      iconPath: Theme.dashboard.connectionModuleAuthenticationImage
      metricValue: {
        if(showWireguard)
          return "Poly1305"
        if(Daemon.displayConnectionConfig.openvpnCipher.endsWith("GCM"))
          return "GCM"
        return Daemon.displayConnectionConfig.openvpnAuth
      }
      bottomRow: true
    }
    ConnectionModuleMetric {
      metricName: SettingsMessages.handshakeSetting
      iconPath: Theme.dashboard.connectionModuleHandshakeImage
      metricValue: {
        if(showWireguard)
          return "Noise_IK"
        return Daemon.displayConnectionConfig.openvpnServerCertificate
      }
      bottomRow: true
    }

  }



}
