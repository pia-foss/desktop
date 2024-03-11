// Copyright (c) 2024 Private Internet Access, Inc.
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
import QtGraphicalEffects 1.0
import QtQuick.Window 2.10
import "../theme"
import "../common"
import "../core"
import "../daemon"
import "qrc:/javascript/keyutil.js" as KeyUtil

// Notification for when a reconnect is needed to apply settings
Item {
  id: reconnectWarning
  implicitWidth: reconnectWarningImage.width + reconnectWarningText.contentWidth + 2 * Theme.popup.tipTextMargin - 3 + 5
  implicitHeight: reconnectWarningText.contentHeight + 2 * (Theme.popup.tipTextMargin - 3)
  opacity: Daemon.state.needsReconnect ? 1 : 0
  visible: opacity > 0

  function activate() {
    Daemon.connectVPN()
  }

  Behavior on opacity {
    NumberAnimation { duration: Theme.animation.quickDuration }
    enabled: reconnectWarning.Window.window.visible
  }

  BorderImage {
    anchors.fill: reconnectWarningRect
    // Radius 5 with offset 3,3
    anchors.leftMargin: -2
    anchors.topMargin: -2
    anchors.rightMargin: -8
    anchors.bottomMargin: -8

    border {left: 9; top: 9; right: 9; bottom: 9}
    horizontalTileMode: BorderImage.Stretch
    verticalTileMode: BorderImage.Stretch
    source: Theme.settings.reconnectShadowImage
  }

  Rectangle {
    id: reconnectWarningRect
    anchors.fill: parent
    color: Theme.popup.tipBackgroundColor
    radius: Theme.popup.tipBalloonRadius

    Image {
      id: reconnectWarningImage
      x: Theme.popup.tipTextMargin - 3
      anchors.verticalCenter: parent.verticalCenter
      source: Theme.dashboard.notificationInfoAlert
      width: sourceSize.width / 2
      height: sourceSize.height / 2
    }
    Text {
      id: reconnectWarningText
      anchors.left: reconnectWarningImage.right
      anchors.leftMargin: 5
      anchors.verticalCenter: parent.verticalCenter
      font.pixelSize: Theme.popup.tipTextSizePx
      color: Theme.popup.tipTextColor
      text: uiTr("Reconnect to apply settings.")
    }
  }

  ButtonArea {
    anchors.fill: parent
    name: reconnectWarningText.text
    onClicked: activate()
    cursorShape: Qt.PointingHandCursor
  }
}
