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
import "../../theme"

Item {
  property string section: ""
  property string headline: ""
  property string paragraph: ""
  height: paragraphText.y + paragraphText.implicitHeight
    StaticText {
      id: sectionText
      text: section
      x: 0
      y: 0
      color: Theme.onboarding.sectionTitleColor
    }

    Rectangle {
      color: Theme.onboarding.sectionDividerColor
      height: 1
      width: 20
      y: 35
      x: 0
    }

    StaticText {
      id: headlineText
      y: 50
      x: 0
      font.pixelSize: 24
      color: Theme.onboarding.sectionHeadlineColor
      text: headline
      width: parent.width
      wrapMode: Text.WordWrap
    }

    StaticText {
      id: paragraphText
      anchors.top: headlineText.bottom
      anchors.topMargin: 18
      font.pixelSize: 14
      lineHeight: 1.1
      x: 0
      text: paragraph
      color: Theme.onboarding.sectionParagraphColor
      width: parent.width
      wrapMode: Text.WordWrap
    }
}
