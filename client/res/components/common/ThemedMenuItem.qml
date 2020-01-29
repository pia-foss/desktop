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
import "../theme"
import "../core"
import "qrc:/javascript/keyutil.js" as KeyUtil
import PIA.NativeAcc 1.0 as NativeAcc

// Themed menu item for use with ThemedMenu.
MenuItem {
  id: menuItem

  // Property values pulled from the top-level ThemedMenu; sane defaults are
  // given so the property evaluation doesn't choke before the menu is set.
  readonly property real itemHeight: menu ? menu.itemHeight : 10

  // Menu respects the item's activeFocusOnTab for keyboard navigation, we have
  // to disable this when the item is hidden or it would still participate in
  // arrow key navigation.
  activeFocusOnTab: visible
  implicitWidth: parent.width

  NativeAcc.ActionMenuItem.name: menuItem.text
  NativeAcc.ActionMenuItem.highlighted: highlighted
  NativeAcc.ActionMenuItem.onActivated: activate()

  // Hide the item when it's invisible by setting height to 0.  It's possible to
  // dynamically add/remove items from a Menu, but it can only be done
  // imperatively, this is a much simpler way to hook this up to declarative
  // logic.
  implicitHeight: visible ? itemHeight : 0
  contentItem: Text {
    leftPadding: 14
    rightPadding: menuItem.arrow.width
    text: menuItem.text
    font.pixelSize: Theme.dashboard.menuTextPx
    opacity: menuItem.enabled ? 1.0 : 0.3
    color: Theme.dashboard.menuTextColor
    horizontalAlignment: Text.AlignLeft
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
  }
  background: Rectangle {
    opacity: menuItem.enabled ? 1 : 0.3
    color: menuItem.highlighted ? Theme.dashboard.menuHighlightColor : "transparent"
    radius: 5
  }

  function activate() {
    // This is what AbstractButton normally does for spacebar events, we have
    // to replicate it for this to work.  The first case handles menu items
    // created as a delegate for Actions, the second is for raw menu items
    // created directly.
    if(action)
      action.trigger(menuItem)
    else
      triggered()
  }

  // Qt only allows Space for some reason
  Keys.onPressed: {
    if(KeyUtil.handlePartialButtonKeyEvent(event))
      activate()
  }
}
