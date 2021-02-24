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
import "../../../common"
import "../../../core"
import "../../../theme"

// SeparatorModule is the separator between the "above fold" and "below fold"
// modules.  It doesn't have bookmark/drag functionality like the "real"
// modules, but internally it's just another module, mainly so it can slide
// around the same way as other modules are moved.
Module {
  implicitHeight: 30
  moduleKey: separatorKey

  Rectangle {
    anchors.fill: parent
    color: Theme.dashboard.moduleBorderColor

    StaticText {
      text: uiTranslate("BelowFold", "DEFAULT DISPLAY")
      font.pixelSize: Theme.dashboard.moduleSeparatorTextPx
      color: Theme.dashboard.moduleTextColor
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.verticalCenter: parent.verticalCenter
    }
  }
}
