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
import QtQuick.Window 2.11
import ".." // settings/ for SettingsMessages
import "../../client"
import "../../daemon"
import "../../common"
import "../../theme"
import PIA.NativeHelpers 1.0

RowLayout {
  property string label: ""
  signal editClicked
  readonly property var editButton: _editButton
  function focusButton() {
    _editButton.forceActiveFocus(Qt.MouseFocusReason);
  }

  StaticText {
    text: label
    color: Theme.dashboard.textColor
  }

  SettingsButton {
    id: _editButton
    Layout.leftMargin: 5
    horizontalPadding: 15
    icon: "edit"
    text: uiTr("EDIT")

    onClicked: {
      editClicked()
    }
  }
}
