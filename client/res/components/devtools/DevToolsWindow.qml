// Copyright (c) 2019 London Trust Media Incorporated
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
import QtQuick.Window 2.10
import "../common"

SecondaryWindow {
  title: "DaemonTools"

  windowLogicalWidth: 650
  windowLogicalHeight: 650
  contentLogicalWidth: actualLogicalWidth
  contentLogicalHeight: actualLogicalHeight
  resizeable: true

  // The dev tools window does not flip for RTL.
  // It uses several QtQuick.Controls types (ComboBox, TabButton, Button, etc.)
  // that require theming to mirror correctly.  (By default they use regular
  // Text / Image objects; we have to replace all those with the fixed types in
  // 'core' for RTL to work correctly.  The client UI does this for ComboBox and
  // Button, but for some types like TabButton it doesn't appear to be
  // possible.)
  rtlMirror: false

  TabBar {
    id: devToolsTabs
    width: parent.width
    anchors.top: parent.top
    TabButton {
      text: "Snapshot"
    }
    TabButton {
      text: "Actions"
    }
    TabButton {
      text: "Paths"
    }
  }

  StackLayout {
    width: parent.width
    anchors.top: devToolsTabs.bottom
    anchors.bottom: parent.bottom
    currentIndex: devToolsTabs.currentIndex

    StateViewTool {
      width: parent.width
      height: parent.height
    }
    ActionsTool {
      width: parent.width
      height: parent.height
    }
    PathsTool {
      width: parent.width
      height: parent.height
    }
  }

  Shortcut {
    sequence: "Ctrl+Shift+I"
    context: Qt.ApplicationShortcut
    onActivated: open()
  }
}
