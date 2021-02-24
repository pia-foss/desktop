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
import "../../../theme"
import "../../../core"
import "../../../common"
import "../../../client"

Item {
  property string metricName: ""
  property string metricValue: ""
  property string iconPath: ""

  // Achieve a row spacing of 5 between rows without any gaps by adding 2 px
  // margin to the top of cells other than the top row, and 3 px margin to the
  // bottom of cells other than the bottom row.
  //
  // We don't want any gap between the cells, because it creates visual noise
  // as the cursor scans over the cells due to the extra transitions back to
  // "CONNECTION" before going to the next label.

  // Whether this is a top-row cell (eliminates the top margin)
  property bool topRow: false
  // Whether this is a bottom-row cell (eliminates the bottom margin)
  property bool bottomRow: false

  readonly property int topMargin: topRow ? 0 : 2
  readonly property int bottomMargin: bottomRow ? 0 : 3

  implicitWidth: 110
  implicitHeight: 20 + topMargin + bottomMargin

  StaticImage {
    x: 0
    y: topMargin
    width: 20
    height: 20
    label: metricName
    source: iconPath
  }

  ValueText {
    x: 24
    y: topMargin
    text: metricValue
    label: metricName
    color: Theme.dashboard.textColor
  }

  MouseArea {
    anchors.fill: parent
    hoverEnabled: true
    onEntered: {
      mouseOverMetric = Client.localeUpperCase(metricName);
    }
    onExited: {
      mouseOverMetric = null;
    }
  }
}
