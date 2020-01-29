// Copyright (c) 2020 Private Internet Access, Inc.
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
import "../../../../common"
import "../../../../core"
import "../../../../theme"

// SettingsButtonBase is the base implementation of the normal (toggle) and
// action buttons. It displays the visual state of the control, but doesn't
// provide interactivity, since that depends on whether the button toggles a
// setting or takes an action directly.
Item {
  id: settingsButton

  // Name of the setting displayed in the Settings module when the button is
  // pointed.
  property string displayName
  // Current setting value displayed by the button.
  property bool currentValue
  // Whether this setting can be toggled right now.  (The current state is still
  // displayed when the button is not enabled.)
  property bool settingEnabled: true
  // Icon resource type for this button
  property string iconResourceType
  // The mouse area for this button - pressed, containsMouse, and containsPress
  // determine the icon state drawn
  property GenericButtonArea mouseArea
  readonly property bool hovered: mouseArea.containsMouse

  width: 32
  height: 32

  // Background circle
  // (QML doesn't have a circle/ellipse type, but a Rectangle with
  // width == height and radius == width/2 becomes a circle)
  Rectangle {
    anchors.fill: parent
    radius: width / 2
    visible: settingEnabled && (mouseArea.containsMouse || mouseArea.pressed)

    color: {
      if(mouseArea.containsPress)
        return currentValue ? Theme.dashboard.settingOnPressBorderColor : Theme.dashboard.settingOffPressBorderColor
      if(mouseArea.containsMouse || mouseArea.pressed)
        return currentValue ? Theme.dashboard.settingOnHoverBorderColor : Theme.dashboard.settingOffHoverBorderColor
      return '#000000'
    }

    // When disabled, show an inner fill to turn the circle above into just a rim
    Rectangle {
      anchors.fill: parent
      anchors.margins: 2
      radius: width / 2
      color: {
        if (mouseArea.containsPress)
          return currentValue ? Theme.dashboard.settingOnPressFillColor : Theme.dashboard.settingOffPressFillColor
        return Theme.dashboard.backgroundColor
      }
    }
  }

  // Off/disabled image
  Image {
    anchors.centerIn: parent
    visible: !settingEnabled || !(currentValue || mouseArea.containsMouse || mouseArea.pressed)
    opacity: settingEnabled ? 1.0 : 0.5
    source: Theme.dashboard.settingOffImage(iconResourceType)
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }
  // Off hover image
  Image {
    anchors.centerIn: parent
    visible: settingEnabled && !currentValue && (mouseArea.containsMouse ^ mouseArea.pressed)
    source: Theme.dashboard.settingOffHoverImage(iconResourceType)
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }
  // Off press image
  Image {
    anchors.centerIn: parent
    visible: settingEnabled && !currentValue && mouseArea.containsPress
    source: Theme.dashboard.settingOffPressImage(iconResourceType)
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }
  // On image
  Image {
    anchors.centerIn: parent
    visible: settingEnabled && currentValue && !(mouseArea.containsMouse || mouseArea.pressed)
    source: Theme.dashboard.settingOnImage(iconResourceType)
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }
  // On hover image
  Image {
    anchors.centerIn: parent
    visible: settingEnabled && currentValue && (mouseArea.containsMouse ^ mouseArea.pressed)
    source: Theme.dashboard.settingOnHoverImage(iconResourceType)
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }
  // On press image
  Image {
    anchors.centerIn: parent
    visible: settingEnabled && currentValue && mouseArea.containsPress
    source: Theme.dashboard.settingOnPressImage(iconResourceType)
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }
}
