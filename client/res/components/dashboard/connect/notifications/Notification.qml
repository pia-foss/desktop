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
import "../../../theme"
import "../../../common"
import "../../../core"
import PIA.NativeAcc 1.0 as NativeAcc

// Notification displays a single notification item (error / warning / general
// notification) on the connect region of the connect page.
//
// Notification sets its implicitHeight based on the height computed from the
// message text layout.
Item {
  id: notification

  // A NotificationStatus from ClientNotifications to display
  property NotificationStatus status

  implicitHeight: content.implicitHeight + 2 * Theme.dashboard.notificationVertMarginPx
  visible: status.showMessage

  readonly property var severityNames: {
    var names = {}
    //: Screen reader annotation for the "info" icon used for messages
    names[ClientNotifications.severities.info] = uiTr("Info")
    //: Screen reader annotation for the "warning" icon used for messages
    names[ClientNotifications.severities.warning] = uiTr("Warning")
    //: Screen reader annotation for the "error" icon used for messages
    names[ClientNotifications.severities.error] = uiTr("Error")
    return names
  }

  readonly property var severityImages: {
    var imgs = {}
    imgs[ClientNotifications.severities.info] = Theme.dashboard.notificationInfoAlert
    imgs[ClientNotifications.severities.warning] = Theme.dashboard.notificationWarningAlert
    imgs[ClientNotifications.severities.error] = Theme.dashboard.notificationErrorAlert
    return imgs
  }
  readonly property var severityLinkColors: {
    var colors = {}
    colors[ClientNotifications.severities.info] = Theme.dashboard.notificationInfoLinkColor
    colors[ClientNotifications.severities.warning] = Theme.dashboard.notificationWarningLinkColor
    colors[ClientNotifications.severities.error] = Theme.dashboard.notificationErrorLinkColor
    return colors
  }
  readonly property var severityLinkHoverColors: {
    var colors = {}
    colors[ClientNotifications.severities.info] = Theme.dashboard.notificationInfoLinkHoverColor
    colors[ClientNotifications.severities.warning] = Theme.dashboard.notificationWarningLinkHoverColor
    colors[ClientNotifications.severities.error] = Theme.dashboard.notificationErrorLinkHoverColor
    return colors
  }

  //: Screen reader annotation used for Connect page message group, such as
  //: "Warning: Killswitch is enabled".  "%1" is a severity name ("Info"/
  //: "Warning"/"Error"), and "%2" is a status message.
  NativeAcc.Group.name: uiTr("%1: %2").arg(severityNames[status.severity]).arg(status.message)

  Rectangle {
    anchors.fill: parent
    visible: notificationMouseArea.containsMouse
    color: Theme.dashboard.notificationHoverBackgroundColor
  }

  StaticImage {
    id: alertIcon

    x: Theme.dashboard.notificationHorzMarginPx
    // Center vertically on the first line of text
    // Round just so the image renders crisply
    y: Math.round(Theme.dashboard.notificationVertMarginPx + Theme.dashboard.notificationTextLinePx / 2 - height / 2)
    width: notification.status.displayIcon ? sourceSize.width / 2 : 0
    height: sourceSize.height / 2
    visible: notification.status.displayIcon

    label: severityNames[notification.status.severity]

    source: severityImages[notification.status.severity]
  }

  // Close icon - shown only when dismissible
  Image {
    id: closeIcon

    x: parent.width - Theme.dashboard.notificationHorzMarginPx - width
    // Round this Y-coordinate - this ends up determining the Y-coordinate of
    // an InfoTip, and non-integer coordinates for layered components cause
    // artifacts on some Windows backends.  (The InfoTip rounds its own offset
    // from its parent, but it can't round the parent's position, so it could
    // still end up with a non-integer coordinate.)
    y: Math.round(Theme.dashboard.notificationVertMarginPx + Theme.dashboard.notificationTextLinePx / 2 - height / 2)
    width: sourceSize.width / 2
    height: sourceSize.height / 2
    visible: notification.status.dismissible
    opacity: closeMouseArea.containsMouse ? 1.0 : 0.5
    source: Theme.dashboard.notificationCloseImage

    ButtonArea {
      id: closeMouseArea
      anchors.centerIn: parent
      width: 24
      height: 24

      //: Screen reader annotation for the "X" button on dismissible messages.
      //: This removes the message until it triggers again. "%1" is a message,
      //: such as "Killswitch enabled."
      name: uiTr("Dismiss message: %1").arg(status.message)

      hoverEnabled: true
      enabled: notification.status.dismissible
      cursorShape: Qt.PointingHandCursor
      // Dismissible notifications provide a dismiss() method.  This isn't on
      // the base NotificationStatus, so dismissible must be enabled only for
      // notifications that provide this method.
      onClicked: notification.status.dismiss()
    }
  }

  // Content - aligned between the notification icon and close/info button
  // Use a Column, not a ColumnLayout, because we need to dynamically toggle the
  // visible flag on the progress group, and we don't need stretching/shrinking
  // behavior on the height of the column.
  Column {
    id: content
    anchors.left: alertIcon.right
    anchors.leftMargin: Theme.dashboard.notificationImgTextGapPx
    anchors.right: closeIcon.left
    anchors.rightMargin: Theme.dashboard.notificationImgTextGapPx
    anchors.top: parent.top
    anchors.topMargin: Theme.dashboard.notificationVertMarginPx
    spacing: Theme.dashboard.notificationProgressMarginPx

    MessageWithLinks {
      id: messageText

      width: parent.width
      font.pixelSize: Theme.dashboard.notificationTextPx
      wrapMode: Text.Wrap
      color: Theme.dashboard.notificationTextColor
      lineHeight: Theme.dashboard.notificationTextLinePx
      lineHeightMode: Text.FixedHeight

      message: status.message
      links: status.links
      embedLinkClicked: status.embedLinkClicked
      linkColor: severityLinkColors[status.severity]
      linkHoverColor: severityLinkHoverColors[status.severity]
      linkFocusColor: Theme.dashboard.notificationBackgroundColor
      linkFocusBgColor: Theme.popup.focusCueColor
      cursorShape: notificationMouseArea.cursorShape

      // For clickable notifications, the whole-item button is the main
      // accessibility element instead of the message
      messageAccessible: !status.clickable
    }

    RatingControl {
      visible: status.ratingEnabled
      width: parent.width
      height: Theme.dashboard.ratingImageHeightPx
      function onRatingFinished (value) {
        status.ratingFinished(value);
      }
    }

    // Progress controls - the progress bar and stop button.
    RowLayout {
      spacing: Theme.dashboard.notificationProgressMarginPx
      visible: status.progress >= 0 && status.progress <= 100
      width: parent.width

      // Progress bar
      Rectangle {
        Layout.fillWidth: true
        height: Theme.dashboard.notificationProgressHeightPx
        color: Theme.dashboard.notificationProgressBackgroundColor

        NativeAcc.ProgressBar.name: status.message
        NativeAcc.ProgressBar.minimum: 0
        NativeAcc.ProgressBar.maximum: 100
        NativeAcc.ProgressBar.value: status.progress

        Rectangle {
          anchors.top: parent.top
          anchors.bottom: parent.bottom
          anchors.left: parent.left
          width: parent.width * status.progress / 100
          color: Theme.dashboard.notificationProgressColor
        }
      }

      // Stop button
      Rectangle {
        width: Theme.dashboard.notificationProgressStopSizePx
        height: width
        color: {
          if(stopMouseArea.containsPress)
            return Theme.dashboard.notificationStopPressColor
          if(stopMouseArea.containsMouse)
            return Theme.dashboard.notificationStopHoverColor
          return Theme.dashboard.notificationStopColor
        }

        ButtonArea {
          id: stopMouseArea
          anchors.centerIn: parent
          // Make this selectable in the margins around the stop button too,
          // since the stop button is somewhat small.
          width: Theme.dashboard.notificationProgressStopSizePx + 2*Theme.dashboard.notificationProgressMarginPx
          height: width

          //: Screen reader annotation for the square "Stop" button on the
          //: message used to download an update.
          name: uiTr("Stop")

          hoverEnabled: true
          onClicked: status.stop()
        }
      }
    }
  }

  // The whole-item ButtonArea has to stack above the MessageWithLinks to ensure
  // that it can take the mouse events when enabled.
  ButtonArea {
    id: notificationMouseArea
    anchors.fill: parent

    name: status.message
    enabled: status.clickable
    visible: status.clickable
    hoverEnabled: true
    cursorShape: status.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
    onClicked: { status.clicked(); }
    focusCueInside: true
  }

  // The InfoTip still works when the link is clickable, it's on top of the
  // whole-item ButtonArea
  InfoTip {
    anchors.centerIn: closeIcon
    visible: notification.status.tipText
    tipText: notification.status.tipText
    cursorShape: notificationMouseArea.cursorShape
    propagateClicks: false
  }
}
