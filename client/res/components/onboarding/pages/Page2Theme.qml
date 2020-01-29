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
import QtGraphicalEffects 1.0
import "../../theme"
import "../../client"
import "../../common"
import "../../core"


Item {
  anchors.fill: parent

  HeadlineItem {
    id: headline
    // Headline
    y: 66
    x: 58
    width: 280

    section: uiTr("GETTING STARTED")
    headline: uiTr("Choose your theme")
    paragraph: uiTr("Private Internet Access comes with light and dark themes to fit the look and feel of your desktop.")
  }

  readonly property bool dark: Client.settings.themeName === 'dark'
  readonly property bool light: Client.settings.themeName === 'light'

  Item {
    id: darkThemeButton
    anchors.top: headline.bottom
    anchors.topMargin: 30
    x: 58
    width: 300
    height: 50

    Image {
      anchors.fill: parent
      source: dark ? Theme.onboarding.themeButtonSelectedImage : Theme.onboarding.themeButtonDeselectedImage
      rtlMirror: true
    }

    Text {
      id: darkThemeText
      anchors.verticalCenter: parent.verticalCenter
      x: 25
      text: uiTr("Dark Theme")
      color: Theme.onboarding.themeButtonTextColor
    }

    RadioButtonArea {
      anchors.fill: parent
      name: darkThemeText.text
      checked: dark
      cursorShape: Qt.PointingHandCursor
      onClicked: {
        Client.applySettings({'themeName':'dark'});
      }
    }
  }

  Item {
    id: lightThemeButton
    anchors.top: darkThemeButton.bottom
    anchors.topMargin: 20
    x: 58
    width: 300
    height: 50

    Image {
      anchors.fill: parent
      source: light ? Theme.onboarding.themeButtonSelectedImage : Theme.onboarding.themeButtonDeselectedImage
      rtlMirror: true
    }

    Text {
      id: lightThemeText
      anchors.verticalCenter: parent.verticalCenter
      x: 25
      text: uiTr("Light Theme")
      color: Theme.onboarding.themeButtonTextColor
    }

    RadioButtonArea {
      anchors.fill: parent
      name: lightThemeText.text
      checked: light
      cursorShape: Qt.PointingHandCursor
      onClicked: {
        Client.applySettings({'themeName':'light'});
      }
    }
  }

  DropShadow {
    visible: light
    anchors.fill: lightThemeButton
    source: lightThemeButton
    color: Theme.onboarding.themeSelectorDropShadow
    horizontalOffset: 0
    verticalOffset: 5
    radius: 10
    spread: 0
  }

  DropShadow {
    visible: dark
    // We cannot change the source/fill dynamically depending on
    // the theme, because that causes a crash in software mode.
    anchors.fill: darkThemeButton
    source: darkThemeButton
    color: Theme.onboarding.themeSelectorDropShadow
    horizontalOffset: 0
    verticalOffset: 5
    radius: 10
    spread: 0
  }

  StaticImage {
    x: 413
    y: 95
    height: 345
    width: 230
    label: uiTr("Theme preview")
    source: Theme.onboarding.themePreviewImage
  }

}
