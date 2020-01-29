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
import "../../theme"
import "../../common"
import "../../core"
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
    width: 368
    height: 38
    anchors.horizontalCenter: parent.horizontalCenter
  }

  Image {
    y: 219
    anchors.horizontalCenter: parent.horizontalCenter
    width: 130
    height: 134
    source: Theme.onboarding.spacemanImage
  }

  Item {
    focus: true
    y: 402
    anchors.horizontalCenter: parent.horizontalCenter
    width: 268
    height: 48

    Image {
      anchors.fill: parent
      source: Theme.onboarding.primaryButtonImage
    }

    Text {
      id: quickTourText
      anchors.horizontalCenter: parent.horizontalCenter
      text: uiTr("QUICK TOUR")
      y: 13
      color: Theme.onboarding.primaryButtonTextColor
    }

    ButtonArea {
      anchors.fill: parent
      name: quickTourText.text
      cursorShape: Qt.PointingHandCursor
      onClicked: {
        currentPage = 1
      }
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
