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
import "../../common"
import "../../core"
import "../../theme"

Item {
  HeadlineItem {
    // Headline
    y: 66
    x: 58
    width: 280
    id: headline

    section: uiTr("GOOD TO GO")
    headline: uiTr("VPN Protection Everywhere You Need It")
    paragraph: uiTr("Your Private Internet Access account can protect up to 10 different devices.")
  }

  StaticImage {
    source: Theme.onboarding.platformsImage
    x: 64
    height: 104
    width: 530
    anchors.top: headline.bottom
    anchors.topMargin: 45
    label: uiTr("Supported platforms")
  }
}
