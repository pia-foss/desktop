// Copyright (c) 2024 Private Internet Access, Inc.
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
import "../../../../common"
import "../../../../settings/stores"

// SettingsToggleButton represents a boolean setting that's toggled when it is
// clicked.  The setting is controlled by a Setting object specified in the
// 'setting' property.
SettingsButtonBase {
  id: settingsButton

  // Setting displayed and toggled by the button.
  property Setting setting

  currentValue: setting.currentValue
  mouseArea: checkButtonArea

  // The MouseArea is *not* disabled when the setting is disabled, because we
  // still detect hover events to display the name of the setting.
  CheckButtonArea {
    id: checkButtonArea
    anchors.fill: parent
    cursorShape: settingEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    hoverEnabled: true
    // Manual 'disabled' hint since we don't actually disable the MouseArea
    accDisabled: !settingEnabled

    name: settingsButton.displayName
    checked: setting.currentValue

    onClicked: {
      if(settingEnabled)
        setting.currentValue = !setting.currentValue
    }
  }
}
