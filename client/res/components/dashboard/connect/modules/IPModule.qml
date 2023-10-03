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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../../../javascript/app.js" as App
import "../../../../javascript/util.js" as Util
import "../../../common"
import "../../../core"
import "../../../daemon"
import "../../../theme"
import "../../../helpers"
import PIA.NativeAcc 1.0 as NativeAcc

MovableModule {
  id: ipModule
  moduleKey: 'ip'

  implicitHeight: portForward.showPf ? (portForward.y + portForward.height + 20) : 80

  //: Screen reader annotation for the tile displaying the IP addresses.
  tileName: uiTr("IP tile")
  NativeAcc.Group.name: tileName

  ConnStateHelper {
    id: connState
  }

  property real vpnElementsOpacityTarget: {
    switch(connState.connectionState) {
    default:
    case connState.stateDisconnecting:
    case connState.stateDisconnected:
    case connState.stateConnecting:
      return 0.3
    case connState.stateConnected:
      return 1
    }
  }
  property real vpnElementsOpacity: vpnElementsOpacityTarget
  Behavior on vpnElementsOpacity {
    NumberAnimation {
      easing.type: Easing.InOutQuad
      duration: 300
    }
  }

  LabelText {
    id: ipLabel
    text: uiTr("IP")
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 20
  }

  CopiableValueText {
    id: currentTextLabel

    copiable: !!Daemon.state.externalIp
    text: Daemon.state.externalIp || "---"
    label: ipLabel.text
    color: {
      if(connState.connectionState === connState.stateDisconnected ||
         connState.connectionState === connState.stateDisconnecting)
        return Theme.dashboard.moduleTitleColor
      return Theme.dashboard.moduleTextColor
    }
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: 20
    y: 40
  }

  LabelText {
    id: vpnIpLabel
    text: uiTr("VPN IP")
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 170
    y: 20
    opacity: vpnElementsOpacity
  }

  CopiableValueText {
    id: vpnIpValue
    copiable: !!Daemon.state.externalVpnIp

    text: Daemon.state.externalVpnIp || "---"
    label: vpnIpLabel.text
    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: 170
    y: 40
    opacity: vpnElementsOpacity
  }

  Image {
    id: arrow
    source: Theme.dashboard.moduleRightArrowImage
    width: 25
    height: 25
    y: 25
    x: 130
    rtlMirror: true
    opacity: vpnElementsOpacity
  }

  StaticImage {
    id: portForwardImg

    opacity: portForward.opacity
    visible: portForward.visible
    anchors.verticalCenter: portForward.verticalCenter
    x: 170
    width: sourceSize.width / 2
    height: sourceSize.height / 2

    //: Screen reader annotation for the arrow graphic that represents the
    //: "port forward" status, which is enabled by the "Port Forwarding"
    //: setting.
    label: uiTr("Port forward")

    source: {
      switch(Daemon.state.forwardedPort) {
      default:
      case Daemon.state.portForward.inactive:
      case Daemon.state.portForward.attempting:
        return Theme.dashboard.ipPortForwardImage
      case Daemon.state.portForward.failed:
      case Daemon.state.portForward.unavailable:
        return Theme.dashboard.ipPortForwardSlashImage
      }
    }
  }

  CopiableValueText {
    id: portForward
    copiable: Daemon.state.forwardedPort > 0

    anchors.left: portForwardImg.right
    anchors.leftMargin: 3
    y: 65

    // PF is "active" when the setting is enabled or if the port forward
    // is anything other than "inactive" (it could be active, but not enabled
    // if it was turned off after a port was already forwarded).
    readonly property bool pfActive: (Daemon.settings.portForward || Daemon.state.forwardedPort !== Daemon.state.portForward.inactive)
    readonly property bool showPf: pfActive

    opacity: showPf ? vpnElementsOpacityTarget : 0.0
    visible: opacity > 0.0
    text: {
      if(!showPf)
        return ""

      switch(Daemon.state.forwardedPort) {
        default:
          return Daemon.state.forwardedPort // Port was forwarded, show the number
        case Daemon.state.portForward.inactive:
        case Daemon.state.portForward.attempting:
          return "---"
        case Daemon.state.portForward.failed:
          //(comments with "//:" are translator comments for the text)
          //: Port forward - label used in IP widget when request fails
          return uiTr("Failed")
        case Daemon.state.portForward.unavailable:
          //: Port forward - label used in IP widget when not available for this region
          return uiTr("Not Available")
      }
    }
    label: portForwardImg.label
    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx

    Behavior on opacity {
      NumberAnimation {
        easing.type: Easing.InOutQuad
        duration: 300
      }
    }
  }
}
