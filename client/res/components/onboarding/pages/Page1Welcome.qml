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
import "../../theme"
import "../../common"
import "../../core"
import "../../client"
import PIA.NativeHelpers 1.0

Item {
  anchors.fill: parent
  StaticText {
    text: uiTr("Welcome to")
    color: Theme.onboarding.defaultTextColor
    y: 60
    anchors.horizontalCenter: parent.horizontalCenter
  }

  StaticImage {
    y: 98
    label: NativeHelpers.productName
    source: Theme.onboarding.logoImage
    readonly property double scaleFactor: 0.6
    width: sourceSize.width * scaleFactor
    height: sourceSize.height * scaleFactor
    anchors.left: parent.left
    anchors.leftMargin: 286
  }

  Image {
    y: 219
    anchors.horizontalCenter: parent.horizontalCenter
    readonly property double scaleFactor: 0.5 * 0.5 // Source image is at 2x. Scale down by additional 50% to compensate
    width: sourceSize.width * scaleFactor
    height: sourceSize.height * scaleFactor
    source: Theme.onboarding.spacemanImage
  }

  PagePrimaryButton {
    focus: true
    y: 402
    anchors.horizontalCenter: parent.horizontalCenter
    text: uiTr("QUICK TOUR")
    onClicked: {
      Client.uiState.onboarding.currentPage = 1
    }
  }

  Item {
    y: 470
    focus: true
    anchors.horizontalCenter: parent.horizontalCenter
    width: 260
    height: 40

    Image {
      anchors.fill: parent
      source: Theme.onboarding.secondaryButtonImage
    }

    Text {
      id: logInText
      anchors.horizontalCenter: parent.horizontalCenter
      text: uiTr("LOG IN")
      color: Theme.onboarding.secondaryButtonTextColor
      y: 10
    }
    ButtonArea {
      anchors.fill: parent
      name: logInText.text
      cursorShape: Qt.PointingHandCursor
      onClicked: {
        closeAndShowDashboard()
      }
    }
  }
}
