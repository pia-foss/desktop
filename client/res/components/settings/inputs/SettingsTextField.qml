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
import QtQuick.Controls 2.4
import "../../theme"
import "../../common"
import "../../core"

ThemedTextField {
  property int textBoxVerticalPadding: 0
  id: control

  readonly property color defaultBorderColor: Theme.settings.inputTextboxBorderColor
  property color borderColor: defaultBorderColor

  font.pixelSize: 13
  color: control.enabled ? Theme.settings.inputTextboxTextColor : Theme.settings.inputTextboxTextDisabledColor
  topPadding: textBoxVerticalPadding
  bottomPadding: textBoxVerticalPadding
  leftPadding: 7
  rightPadding: 7

  background: Rectangle {
    radius: 3
    color: Theme.settings.inputTextboxBackgroundColor
    border.width: control.activeFocus ? 2 : 1
    border.color: control.borderColor
    implicitWidth: 100
    implicitHeight: 24 + 2*textBoxVerticalPadding
  }
}
