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
import QtGraphicalEffects 1.0
import "../../theme"
import "../../client"
import "../../common"
import "../../core"

Item {
  HeadlineItem {
    // Headline
    y: 66
    x: 58
    width: 280

    section: uiTr("PRO TIP")
    headline: uiTr("Customize Your VPN Experience")
    paragraph: uiTr("Choose from a variety of tiles to customize your dashboard to display the information and controls most relevant to you.")
  }

  readonly property bool dark: Client.settings.themeName === 'dark'
  readonly property bool light: Client.settings.themeName === 'light'

  BorderImage {
    anchors.fill: customizeImage
    anchors.margins: -15

    border {left: 25; top: 25; right: 25; bottom: 25}
    horizontalTileMode: BorderImage.Stretch
    verticalTileMode: BorderImage.Stretch
    source: Theme.onboarding.customizeShadowImage
  }

  StaticImage {
    id: customizeImage
    readonly property double imageScaleFactor: 0.8
    width: 300 * imageScaleFactor
    height: 444 * imageScaleFactor
    y: 51
    x: 413
    label: uiTr("Customization preview")
    source: Theme.onboarding.customizeImage
  }
}
