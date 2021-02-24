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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.11
import "../../javascript/app.js" as App
import "../theme"
import "../daemon"
import "../client"

Item {
  id: wrapper
  property alias headerTopColor: headerBar.topColor
  // Account for the header bar when passing the layout height down and the
  // pages' current and maximum heights up
  property int layoutHeight
  readonly property int pageHeight: pageManager.pageHeight + headerBar.height
  readonly property int maxPageHeight: pageManager.maxPageHeight + headerBar.height
  property real contentRadius

  // Clip the contents to the page bound, so the sliding pages during a
  // transition don't draw outside on the window margins
  Item {
    id: contentClip
    anchors.fill: parent
    // Extend the clip bound to the right when dragging a module
    anchors.rightMargin: -pageManager.clipRightExtend
    clip: true

    // Fit the contents to the non-extended bound
    Item {
      id: contentBound
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.bottom: parent.bottom
      width: wrapper.width

      Rectangle {
        anchors.fill: parent
        // Drop the top of this rectangle by the corner radius, so the header's
        // rounded corners don't blend with it.
        // (The header renders the background color when no colored header is
        // shown.)
        // Put it back at the top edge on Windows when sliding the dashboard
        // though, due to the way opacity is composited by Qt the hack would
        // become visible, and fringing isn't really noticeable during the slide.
        // (Enabling layered rendering in the right places would eliminate this,
        // but that causes High-DPI scaling to become bitmap-stretched because of
        // the way the scale transform is applied.)
        anchors.topMargin: Window.window.slideBlending ? 0 : wrapper.contentRadius
        color: Theme.dashboard.backgroundColor
        radius: wrapper.contentRadius
      }

      HeaderBar {
        id: headerBar
        height: 60
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        backButtonDescription: pageManager.backButtonDescription
        backButtonFunction: pageManager.backButtonFunction
      }

      PageManager {
        id: pageManager
        anchors.top: headerBar.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        layoutHeight: wrapper.layoutHeight - headerBar.height
      }
    }
  }

  Shortcut {
    sequence: "Ctrl+L"
    context: Qt.ApplicationShortcut
    onActivated: Client.startLogUploader()
  }

  Component.onCompleted: {
    headerBar.setPageManager(pageManager)
  }
}
