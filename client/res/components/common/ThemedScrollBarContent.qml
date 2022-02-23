// Copyright (c) 2022 Private Internet Access, Inc.
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
import "../theme"

// This should be used as the content item for scroll bars on
// Flickables/ScrollViews.
Rectangle {
  readonly property ScrollBar scrollBar: parent

  // Scale the scroll bar - used by SettingsWindow since the scroll bars are
  // outside the scale wrapper
  property real scale: 1.0
  readonly property real barThickness: 6*scale

  implicitWidth: scrollBar.horizontal ? scrollBar.parent.width : barThickness
  implicitHeight: scrollBar.horizontal ? barThickness : scrollBar.parent.height
  radius: barThickness/2
  color: scrollBar.pressed ? Theme.dashboard.scrollBarPressedColor : Theme.dashboard.scrollBarColor

  // Show the scroll bar when it's forced on or when it's possible to scroll
  visible: {
    if(scrollBar.policy === ScrollBar.AlwaysOff)
      return false
    if(scrollBar.policy === ScrollBar.AlwaysOn)
      return true
    return scrollBar.size < 1.0
  }
}
