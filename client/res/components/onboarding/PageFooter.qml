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
import "../theme"
import "../common"
import "../core"

Item {

  Item {
    visible: currentPage < numPages - 1
    Text {
      id: skipTourText
      color: Theme.onboarding.skipTourColor
      text: uiTr("SKIP TOUR")
      anchors.verticalCenter: parent.verticalCenter
    }
    height: 40
    width: 150
    anchors.left: parent.left

    ButtonArea {
      anchors.fill: parent
      name: skipTourText.text
      cursorShape: Qt.PointingHandCursor
      onClicked: {
        closeAndShowDashboard();
      }
    }
  }

  Item {
    width: 58
    height: 10
    anchors.centerIn: parent

    NavigationDot {
      x: 0
      targetPage: 1
    }
    NavigationDot {
      x: 24
      targetPage: 2
    }
    NavigationDot {
      x: 48
      targetPage: 3
    }
  }

  Item {
    anchors.right: parent.right
    width: 150
    height: 40
    property bool lastPageFlag: currentPage === (numPages - 1)

    Image {
      anchors.fill: parent
      source: Theme.onboarding.nextButtonImage
    }

    Text {
      id: nextButtonText
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.verticalCenter: parent.verticalCenter
      text: parent.lastPageFlag ? uiTr("LOG IN") : uiTr("NEXT")
      color: Theme.onboarding.buttonTextColor
    }

    ButtonArea {
      anchors.fill: parent
      name: nextButtonText.text
      cursorShape: Qt.PointingHandCursor
      onClicked: {
        if(parent.lastPageFlag) {
          closeAndShowDashboard()
        }
        else {
          currentPage += 1
        }
      }
    }
  }

}
