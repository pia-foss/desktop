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
import "../common"
import "../theme"
import PIA.NativeAcc 1.0 as NativeAcc

Rectangle {
  // use a rectangle to simulate a circle/navigation dot
  x: 0
  y: 0
  height: 10
  width: 10
  radius: 5
  property int targetPage: 0
  readonly property bool active: targetPage === currentPage
  color: active ? Theme.onboarding.navDotActive : Theme.onboarding.navDotInactive

  RadioButtonArea {
    cursorShape: Qt.PointingHandCursor
    anchors.fill: parent

    //: Screen reader annotation for the navigation dots in the Quick Tour.
    //: These indicate pages the user can navigate to; "%1" is a page index from
    //: 1 to 3.
    name: uiTr("Page %1").arg(targetPage)
    checked: active

    onClicked: {
      currentPage = targetPage;
    }
  }
}
