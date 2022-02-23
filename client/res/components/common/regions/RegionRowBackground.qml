// Copyright (c) 2022 Private Internet Access, Inc.
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
import QtQuick.Layouts 1.3
import "../../theme"
import "../../common"
import "../../core"
import "../../settings/stores"
import PIA.NativeAcc 1.0 as NativeAcc

Item {
  id: regionRowBackground

  // Non-hover background color
  property color backgroundColor
  // Hover background color
  property color hoverBackgroundColor
  // Separator color
  property color separatorColor

  // Show the row highlight.  Used to indicate keyboard highlight, and to
  // handle hover events on InfoTips (MouseAreas propagate presses but not hover
  // events).
  property bool showRowHighlight

  signal clicked()

  MouseArea {
    id: regionBaseMouseArea

    anchors.fill: parent
    hoverEnabled: true
    onClicked: regionRowBackground.clicked()
  }

  // Background (not dimmed if the region dims)
  Rectangle {
    anchors.fill: parent
    color: {
      // The InfoTip propagates press/release events, but not ordinary hover due
      // to limitations in MouseArea.
      if(regionBaseMouseArea.containsMouse || showRowHighlight) {
        return hoverBackgroundColor
      }
      return backgroundColor
    }
  }

  // Separator
  Rectangle {
    color: separatorColor
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    height: 1
  }
}
