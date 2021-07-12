// Copyright (c) 2021 Private Internet Access, Inc.
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

import QtQuick 2.10
import QtQuick.Layouts 1.3
import QtQuick.Window 2.10
import "../theme"
import "../common"
import "../core"

import PIA.PreConnectStatus 1.0
import PIA.NativeHelpers 1.0

Rectangle {
  id: splash

  property int contentRadius

  color: Theme.dashboard.backgroundColor
  radius: contentRadius

  // Show tools if we don't connect for 60 seconds
  property bool showTools: false
  Timer {
    id: showToolsTimer
    interval: 60000
    repeat: false
    running: true
    onTriggered: showTools = true
  }

  // Outer margins
  Item {
    anchors.fill: parent
    anchors.margins: Theme.splash.contentMargins
    anchors.bottomMargin: Theme.splash.contentBottomMargin

    Image {
      id: logo

      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      height: sourceSize.height / sourceSize.width * width

      source: Theme.dashboard.splashLogoImage
    }

    Item {
      anchors.top: logo.bottom
      anchors.bottom: links.top
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.topMargin: Theme.splash.contentSpacing
      anchors.bottomMargin: Theme.splash.contentSpacing

      Image {
        id: spinner
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter

        source: Theme.dashboard.connectButtonConnectingImage
        width: height

        NumberAnimation {
          target: spinner
          property: "rotation"
          from: 0
          to: 360
          duration: Theme.splash.spinnerAnimDuration
          running: splash.Window.window.visible
          loops: Animation.Infinite
        }
      }
    }

    RowLayout {
      id: links
      anchors.bottom: parent.bottom
      anchors.horizontalCenter: parent.horizontalCenter
      spacing: Theme.splash.linkSpacing

      property int maxLinkWidth: {
        var max = 0
        max = Math.max(max, sendLogs.implicitWidth)
        max = Math.max(max, reinstall.implicitWidth)
        max = Math.max(max, quit.implicitWidth)
        return max
      }

      TextLink {
        id: sendLogs
        text: uiTr("Send Logs")

        property real animAmt: (NativeHelpers.logToFile && splash.showTools) ? 1.0 : 0.0
        Behavior on animAmt {
          NumberAnimation {
            duration: Theme.animation.quickDuration
            easing.type: Easing.InOutQuad
          }
          enabled: splash.Window.window.visible
        }

        Layout.preferredWidth: animAmt * links.maxLinkWidth
        horzCenter: true
        opacity: animAmt
        visible: opacity > 0
        onClicked: startLogUploader()
      }

      TextLink {
        id: reinstall

        text: uiTr("Reinstall")

        property real animAmt: (PreConnectStatus.canReinstall && splash.showTools) ? 1.0 : 0.0
        Behavior on animAmt {
          NumberAnimation {
            duration: Theme.animation.quickDuration
            easing.type: Easing.InOutQuad
          }
          enabled: splash.Window.window.visible
        }

        Layout.preferredWidth: animAmt * links.maxLinkWidth
        horzCenter: true
        opacity: animAmt
        visible: opacity > 0
        onClicked: PreConnectStatus.reinstall()
      }

      TextLink {
        id: quit

        Layout.preferredWidth: links.maxLinkWidth
        horzCenter: true
        text: uiTr("Quit")

        onClicked: {
          console.info("Quit from splash dashboard link")
          Qt.quit()
        }
      }
    }
  }

  function startLogUploader() {
    // There is no daemon connection when the splash dashboard is shown.
    // Call NativeHelpers.startLogUploader() instead of
    // Client.startLogUploader(), which would try to ask the daemon to write
    // diagnostics, and write a message so it's clear where this came from.
    console.info("Starting log uploader with no daemon connection, can't write diagnostics")
    NativeHelpers.startLogUploader('')
  }

  Shortcut {
    sequence: "Ctrl+L"
    // The main app handles this as an application-wide shortcut, but the splash
    // popup handles it as a window shortcut.
    // If we handle it as an application shortcut, the Shortcut in
    // DashboardWrapper is prevented from 'stealing' it when the main client UI
    // starts up; Qt probably does not intend for duplicate shortcuts to exist
    // at the same time.
    // This is fine since there are no other windows when the splash dashboard
    // is active, and if somehow it did continue to exist after the main client
    // UI comes up, we prefer the main client's handling (which also asks the
    // daemon to write diagnostics).
    context: Qt.WindowShortcut
    onActivated: startLogUploader()
  }
}
