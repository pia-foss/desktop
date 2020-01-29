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
import QtQuick.Layouts 1.3
import "qrc:/javascript/keyutil.js" as KeyUtil
import PIA.CircleMouseArea 1.0
import "../../theme"
import "../../helpers"
import "../../common"
import "../../core"
import PIA.NativeAcc 1.0 as NativeAcc

Item {
  id: cb

  width: Theme.dashboard.connectButtonSizePx
  height: Theme.dashboard.connectButtonSizePx

  activeFocusOnTab: true

  signal clicked();

  // Connection statuses displayed by the connect button
  readonly property var statuses: ({
    disconnectedError: 0,
    disconnected: 1,
    connecting: 2,
    connected: 3,
    disconnecting: 4,
    snoozed: 5
  })

  // Images displayed for various statuses when hovered by the cursor
  readonly property var fixedHoverImages: {
    var imgs = {}
    imgs[statuses.disconnectedError] = Theme.dashboard.connectButtonErrorHoverImage
    imgs[statuses.disconnected] = Theme.dashboard.connectButtonDisconnectedHoverImage
    imgs[statuses.connecting] = null
    imgs[statuses.connected] = Theme.dashboard.connectButtonConnectedHoverImage
    imgs[statuses.disconnecting] = null
    imgs[statuses.snoozed] = Theme.dashboard.connectButtonSnoozedHoverImage
    return imgs
  }

  // Images displayed for various statuses when not hovered
  readonly property var fixedIdleImages: {
    var imgs = {}
    imgs[statuses.disconnectedError] = Theme.dashboard.connectButtonErrorImage
    imgs[statuses.disconnected] = Theme.dashboard.connectButtonDisconnectedImage
    imgs[statuses.connecting] = null
    imgs[statuses.connected] = Theme.dashboard.connectButtonConnectedImage
    imgs[statuses.disconnecting] = null
    imgs[statuses.snoozed] = Theme.dashboard.connectButtonSnoozedImage
    return imgs
  }

  // Image states that rotate - these are the same for hover / idle
  readonly property var rotateImages: {
    var imgs = {}
    imgs[statuses.disconnectedError] = null
    imgs[statuses.disconnected] = null
    imgs[statuses.connecting] = Theme.dashboard.connectButtonConnectingImage
    imgs[statuses.connected] = null
    imgs[statuses.disconnecting] = Theme.dashboard.connectButtonDisconnectingImage
    imgs[statuses.snoozed] = null
    return imgs
  }

  readonly property int status: {
    if(connState.snoozeModeEnabled) {
      if(connState.snoozeState === connState.snoozeDisconnected)
        return statuses.snoozed
    }

    switch (connState.connectionState) {
      default:
      case connState.stateDisconnected:
        // While disconnected, use the red button if an error is active.
        if(ClientNotifications.worstSeverity >= ClientNotifications.severities.error)
          return statuses.disconnectedError
        else
          return statuses.disconnected
      case connState.stateConnecting:
        return statuses.connecting
      case connState.stateConnected:
        return statuses.connected
      case connState.stateDisconnecting:
        return statuses.disconnecting
    }
  }

  //: Screen reader annotation for the Connect button (the large "power symbol"
  //: button).  Used for all states of the Connect button.
  NativeAcc.Button.name: uiTr("Toggle connection")
  NativeAcc.Button.description: {
    if(connState.snoozeModeEnabled) {
      if(connState.snoozeState === connState.snoozeDisconnected)
        //: Description of the Connect button when connection is "Snoozed"
        //: meaning the connection is temporarily disconnected
        return uiTr("Resume from Snooze and reconnect, currently snoozing and disconnected")
    }

    switch(status) {
      case statuses.disconnectedError:
        //: Description of the Connect button in the "error" state.  This
        //: indicates that an error occurred recently.
        return uiTr("Connect to VPN, error has occurred")
      default:
      case statuses.disconnected:
        //: Description of the Connect button in the normal "disconnected" state
        return uiTr("Connect to VPN")
      case statuses.connecting:
        //: Description of the Connect button when a connection is ongoing
        //: (clicking the button in this state disconnects, i.e. aborts the
        //: ongoing connection)
        return uiTr("Disconnect from VPN, connecting")
      case statuses.connected:
        //: Description of the Connect button in the normal "connected" state
        return uiTr("Disconnect from VPN")
      case statuses.disconnecting:
        //: Description of the Connect button while currently disconnecting.
        //: Clicking the button in this state still tries to disconnect (which
        //: has no real effect since it is already disconnecting).
        return uiTr("Disconnect from VPN, disconnecting")
    }
  }
  NativeAcc.Button.onActivated: handleClick()

  ConnStateHelper {
    id: connState
  }

  Image {
    id: outlineImage
    anchors.fill: cb
    source: Theme.dashboard.connectButtonOutlineImage
  }

  function handleClick() {
    cb.forceActiveFocus(Qt.MouseFocusReason)
    cb.clicked()
  }

  // The "fixed" and "rotating" images are rendered with two separate Image
  // objects.  For whatever reason, attempting to reset the rotation to 0 when
  // stopping the rotation is never 100% reliable.  It's not a big deal if the
  // spinning images start at the wrong angle, but it is a big deal if the idle
  // images are shown crooked.
  Image {
    id: fixedImage
    anchors.fill: cb
    // stateImg is the image resource when a fixed image is shown; null
    // otherwise.
    property var stateImg: {
      if(connectMouseArea.containsMouse)
        return fixedHoverImages[cb.status]
      return fixedIdleImages[cb.status]
    }
    source: stateImg || ""
    visible: !!stateImg
  }

  Image {
    id: rotateImage
    anchors.fill: cb
    property var stateImg: rotateImages[cb.status]
    source: stateImg || ""
    visible: !!stateImg

    RotationAnimator {
      id: ra
      target: rotateImage;
      from: 0;
      to: 360;
      duration: 1000
      running: rotateImage.visible
      loops: Animation.Infinite
      // Reset the rotation when the animation starts or stops.
      onStarted: rotateImage.rotation = 0
      onStopped: rotateImage.rotation = 0
    }
  }

  CircleMouseArea {
    id: connectMouseArea
    anchors.fill: cb
    cursorShape: Qt.PointingHandCursor
    onClicked: handleClick()
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: parent
    control: cb
  }

  Keys.onPressed: {
    if(KeyUtil.handleButtonKeyEvent(event)) {
      focusCue.reveal()
      clicked()
    }
  }
}
