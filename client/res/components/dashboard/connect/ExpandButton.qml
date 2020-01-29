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
import "../../common"
import "../../core"
import "../../theme"

Item {
  property bool expanded: false

  signal toggleExpand()

  id: expandButton
  state: expanded ? 'open' : 'closed'
  states: [
    State {
      name: "open"
      PropertyChanges {
        target: chevron
        rotation: 180
      }
    },
    State {
      name: "closed"
      PropertyChanges {
        target: chevron
        rotation: 0
      }
    }
  ]

  transitions: [
    Transition {
      from: "*"
      to: "*"
      RotationAnimation {
        duration: Theme.animation.normalDuration
        direction: RotationAnimation.Counterclockwise
      }
    }
  ]
  Item {
    anchors.fill: parent

    Rectangle {
      height: 1
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      color: Theme.dashboard.moduleBorderColor
    }

    Image {
      id: chevron
      source: mouseArea.containsMouse ? Theme.dashboard.moduleExpandActiveImage : Theme.dashboard.moduleExpandImage
      width: sourceSize.width/2
      height: sourceSize.height/2
      y: 15
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.verticalCenter: parent.verticalCenter
    }

    ButtonArea {
      id: mouseArea
      anchors.fill: parent

      //: Screen reader annotations for the Expand button at the bottom of the
      //: Connect page, which either expands or collapses the dashboard to
      //: show/hide the extra tiles.  This title should be a brief name
      //: (typically one or two words) of the action that the button will take.
      name: expanded ? uiTr("Collapse") : uiTr("Expand")
      //: Screen reader annotations for the Expand button at the bottom of the
      //: Connect page, which either expands or collapses the dashboard to
      //: show/hide the extra tiles.  This title should be a short description
      //: (typically a few words) indicating that the button will show or hide
      //: the extra tiles.
      description: expanded ? uiTr("Hide extra tiles") : uiTr("Show extra tiles")

      cursorShape: Qt.PointingHandCursor
      hoverEnabled: true
      focusCueInside: true
      onClicked: {
        toggleExpand()
      }
    }
  }
}
