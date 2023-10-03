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
import "../core"
import "../daemon"
import "../theme"
import "qrc:/javascript/util.js" as Util

Item {
  id: dipSubtitle

  property string dedicatedIp
  property color dipTagBackground

  implicitWidth: dipTagBorder.x + dipTagBorder.width
  implicitHeight: ipLabel.height

  Text {
    id: ipLabel
    text: dipSubtitle.dedicatedIp
    font.pixelSize: Theme.regions.sublabelTextPx
    color: Theme.regions.dipRegionSublabelColor
  }

  Rectangle {
    id: dipTagBorder
    anchors.fill: dipTagText
    anchors.leftMargin: -dipTagText.borderRadius
    anchors.rightMargin: -dipTagText.borderRadius
    anchors.topMargin: -dipTagText.borderVertMargin
    anchors.bottomMargin: -dipTagText.borderVertMargin
    anchors.left: ipLabel.right
    radius: dipTagText.borderRadius
    color: dipSubtitle.dipTagBackground
    border.color: Theme.regions.dipRegionSublabelColor
    border.width: 1
  }

  Text {
    id: dipTagText
    anchors.left: ipLabel.right
    anchors.leftMargin: 2*borderRadius
    anchors.verticalCenter: ipLabel.verticalCenter
    text: uiTr("DEDICATED IP")
    font.pixelSize: Theme.regions.tagTextPx
    color: Theme.regions.dipRegionSublabelColor

    readonly property int borderVertMargin: 1
    readonly property int borderRadius: (height+2*borderVertMargin)/2
  }
}
