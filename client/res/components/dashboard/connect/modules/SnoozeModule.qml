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
import QtQuick.Controls 2.3
import "../../../common"
import "../../../core"
import "../../../daemon"
import "../../../theme"
import "../../../helpers"
import "../../../settings/stores"
import PIA.NativeAcc 1.0 as NativeAcc
import QtGraphicalEffects 1.0
import PIA.NativeHelpers 1.0

MovableModule {
  implicitHeight: 80
  moduleKey: 'snooze'

  //: Screen reader annotation for the Snooze tile
  tileName: uiTr("VPN Snooze tile")
  NativeAcc.Group.name: tileName

  ConnStateHelper {
    id: connState
  }

  ClientSetting {
    id: snoozeDurationSettings
    name: "snoozeDuration"
  }


  Text {
    text: {
      switch(connState.snoozeState) {
      case connState.snoozeConnecting:
        return uiTr("RESUMING CONNECTION")
      case connState.snoozeDisconnected:
        return uiTr("SNOOZED")
      case connState.snoozeDisconnecting:
        return uiTr("SNOOZING")
      }

      uiTr("VPN SNOOZE")
    }
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 10
    width: 260
    elide: Text.ElideRight
  }

  function onSnoozeButtonClicked () {

    if(connState.canSnooze) {
      Daemon.startSnooze(snoozeAmount);
    } else if (connState.canResumeFromSnooze) {
      Daemon.stopSnooze();
    } else {
      console.warn("Snooze button clicked when unable to start/stop");
    }

  }
  // The amount that we will snooze by if we do start a snooze
  property int snoozeAmount: snoozeDurationSettings.sourceValue
  readonly property int buttonSnoozeChangeAmount: 60
  property int snoozeSecondsRemaining: -1


  readonly property string timerText: {
    var displaySeconds = snoozeAmount;
    // Currently "snoozing", use the real daemon state to
    if(connState.snoozeState === connState.snoozeDisconnected && snoozeSecondsRemaining > 0) {
      displaySeconds = snoozeSecondsRemaining;
    }

    displaySeconds = displaySeconds > 0 ? displaySeconds : 0;

    // otherwise, use the snoozeAmount to determine the
    var mins = Math.floor(displaySeconds / 60);
    var sec = Math.floor(displaySeconds % 60);

    if(sec < 10) {
      // Show 5 sec as "05"
      sec = "0" + sec;
    }

    return mins + ":" + sec;
  }

  Timer {
    id: remainingTimeCalculator
    // Run timer only when in "snoozed" state and connection is down
    running: connState.snoozeState === connState.snoozeDisconnected
    repeat: true
    interval: 1000
    function calculateRemainingTime () {
      var currentTime = NativeHelpers.getMonotonicTime();
      if(Daemon.state.snoozeEndTime > currentTime)
        snoozeSecondsRemaining = Math.floor((Daemon.state.snoozeEndTime - currentTime)/ 1000);
      else
        snoozeSecondsRemaining = 0;
    }

    onTriggered: {
      calculateRemainingTime();
    }
  }

  readonly property int adjustButtonWidth: 30
  readonly property int timeDisplayWidth: 60
  readonly property int buttonHeight: 30
  readonly property int buttonBorderWidth: 2
  readonly property int maxSnoozeDuration: 60*30 // 30 mins max snooze

  property bool changeButtonsEnabled: {
    // disable the change (+/-) buttons when snooze is connecting/disconnecting/snoozed
    return !connState.snoozeModeEnabled
  }
  readonly property bool decrementButtonEnabled: {
    return changeButtonsEnabled && snoozeAmount > buttonSnoozeChangeAmount
  }
  readonly property bool incrementButtonEnabled: {
    return changeButtonsEnabled && snoozeAmount < maxSnoozeDuration
  }

  Rectangle {
    x: 20
    y: 35
    id: snoozeTimer

    color: Theme.dashboard.pushButtonBackgroundColor
    width: 2 * adjustButtonWidth + timeDisplayWidth
    height: buttonHeight
    radius: buttonHeight / 2


    // Shape used for hover effect on decrement (-) button
    Item {
      anchors.verticalCenter: snoozeTimer.verticalCenter
      anchors.left: snoozeTimer.left
      anchors.leftMargin: buttonBorderWidth
      anchors.right: snoozeTimer.horizontalCenter
      anchors.rightMargin: timeDisplayWidth / 2 + buttonBorderWidth
      height: buttonHeight - 2 * buttonBorderWidth
      readonly property color innerColor: {
        // If button is disabled
        if(!decrementButtonEnabled) {
          return Theme.dashboard.backgroundColor;
        }
        return decrementButtonArea.containsMouse ? Theme.dashboard.pushButtonBackgroundHover : "transparent"
      }

      Rectangle {
        id: circularHoverLeft
        color: parent.innerColor
        height: parent.height
        radius: height / 2
        width: height
      }

      Rectangle {
        color: parent.innerColor
        anchors.left: circularHoverLeft.horizontalCenter
        anchors.right: parent.right
        height: parent.height

        Text {
          text: "-"
          color: decrementButtonEnabled ? Theme.dashboard.textColor : Theme.dashboard.moduleSecondaryTextColor
          anchors.verticalCenter: parent.verticalCenter
          anchors.left: parent.left
        }
      }

      ButtonArea {
        id: decrementButtonArea
        anchors.fill: parent
        //: Screen reader name for the "minus" button that decreases snooze time
        name: uiTr("Decrease snooze time")
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        enabled: decrementButtonEnabled

        onClicked: {
          snoozeDurationSettings.currentValue = snoozeAmount - buttonSnoozeChangeAmount;
        }
      }
    }

    Rectangle {
      // Time display
      x: adjustButtonWidth
      y: buttonBorderWidth
      width: timeDisplayWidth
      height: buttonHeight - 2 * buttonBorderWidth
      color: Theme.dashboard.snoozeTimeDisplayColor

      ValueText {
        color: Theme.dashboard.textColor
        //: Screen reader annotation for the snooze time display in the Snooze
        //: tile
        label: uiTr("Snooze time")
        text: timerText
        anchors.centerIn: parent
      }
    }

    // Shape used for hover effect on increment (+) button
    Item {
      anchors.verticalCenter: snoozeTimer.verticalCenter
      anchors.right : snoozeTimer.right
      anchors.rightMargin: buttonBorderWidth
      anchors.left: snoozeTimer.horizontalCenter
      anchors.leftMargin: timeDisplayWidth / 2 + buttonBorderWidth
      height: buttonHeight - 2 * buttonBorderWidth
      readonly property color innerColor: {
        // If button is disabled
        if(!incrementButtonEnabled) {
          return Theme.dashboard.backgroundColor;
        }
        return incrementButtonArea.containsMouse ? Theme.dashboard.pushButtonBackgroundHover : "transparent"
      }

      Rectangle {
        id: circularHoverRight
        color: parent.innerColor
        height: parent.height
        radius: height / 2
        width: height

        anchors.right: parent.right
      }

      Rectangle {
        color: parent.innerColor
        anchors.left: parent.left
        anchors.right: circularHoverRight.horizontalCenter
        height: parent.height

        Text {
          text: "+"
          color: incrementButtonEnabled ? Theme.dashboard.textColor : Theme.dashboard.moduleSecondaryTextColor
          anchors.verticalCenter: parent.verticalCenter
          anchors.right: parent.right
        }
      }

      ButtonArea {
        id: incrementButtonArea
        anchors.fill: parent
        //: Screen reader name for the "plus" button that increases snooze time
        name: uiTr("Increase snooze time")
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        enabled: incrementButtonEnabled
        onClicked: {
          snoozeDurationSettings.currentValue = snoozeAmount + buttonSnoozeChangeAmount;
        }
      }
    }
  }

  PushButton {
    id: snoozeButton
    anchors.left: snoozeTimer.right
    anchors.leftMargin: 10
    anchors.verticalCenter: snoozeTimer.verticalCenter

    height: snoozeTimer.height

    minWidth: 75
    borderSize: buttonBorderWidth

    labels: [uiTr("Snooze"), uiTr("Resume")]
    currentLabel: connState.canResumeFromSnooze ? 1 : 0

    enabled: connState.canResumeFromSnooze || connState.canSnooze

    onClicked: onSnoozeButtonClicked()
  }

  InfoTip {
    id: snoozeTip
    anchors.left: snoozeButton.right
    anchors.leftMargin: 10
    anchors.verticalCenter: snoozeButton.verticalCenter

    tipText: uiTr("Snooze temporarily disconnects the VPN and automatically reconnects when the timer elapses.")
  }

  Component.onCompleted: {
    Daemon.state.onSnoozeEndTimeChanged.connect(function () {
      if(Daemon.state.snoozeEndTime > 0) {
        remainingTimeCalculator.calculateRemainingTime();
      }
    })
  }
}
