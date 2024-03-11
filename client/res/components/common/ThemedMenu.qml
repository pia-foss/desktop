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
import "../theme"
import PIA.NativeAcc 1.0 as NativeAcc

// Menu with the PIA theming applied.  Specifies a themed MenuItem as the
// delegate to represent Actions.
//
// The menu's width and item height should be set to sensible values for that
// particular menu.
Menu {
  id: menu

  // ThemedMenu itself is not annotated for screen readers, only the menu items
  // are.  There is a PopupMenu type (with start/end events), but VoiceOver on
  // Mac doesn't handle it well - it highlights the menu and speaks the number
  // of items correctly, but it can't enter the menu to see the items.
  //
  // The UX with just menu item annotations is pretty good though, it starts on
  // the menu items and speaks as if the user is in a menu (escape to close
  // works, etc.)  Thanks to WindowAccImpl hiding non-overlay content when a
  // modal overlay is active, the user can't "escape" to the non-menu contents
  // while the menu is open.

  property real menuWidth
  property real itemHeight

  implicitWidth: menuWidth
  padding: 1
  background: Rectangle {
    color: Theme.dashboard.menuBackgroundColor
    border.color: Theme.dashboard.menuBorderColor
    radius: 5
  }

  // The Action's text, enabled, and onTriggered properties bind to the
  // ThemedMenuItem
  delegate: ThemedMenuItem {}

  // When the menu is opened, focus the first focusable item.
  // QML Menus interact pretty poorly with screen readers, they just see them as
  // a box of buttons.
  //
  // The screen reader doesn't really know that the user is in a menu, so it
  // still will navigate all over the window and potentially interact with other
  // controls.  The directional navigation is really bad too, if the user leaves
  // the menu, it may be impossible to get back, or the screen reader cursor
  // could get stuck on a specific menu item.
  //
  // The tweaks below make this less bad:
  // - Focus the first item when the menu pops, so the screen reader cursor is
  //   in the right place initially
  // - If the menu loses focus, close it
  //
  // Qt also has a hidden "Value Item" that shows up on the right edge of the
  // menu, but confusingly acts as if it's "below" the menu items (you have to
  // use VO-Up to get back to the items).  There doesn't seem to be anything we
  // can do about that though.
  onOpened: {
    var firstFocus = contentItem.nextItemInFocusChain(true)
    // MouseFocusReason isn't perfect, we don't know exactly why the menu was
    // opened here.
    if(firstFocus)
      firstFocus.forceActiveFocus(Qt.MouseFocusReason)
  }

  property bool lastActiveFocus: false
  onActiveFocusChanged: {
    // Close when focus is lost.
    // This is also important to ensure the menu is dismissed if the user
    // switches to another application.
    if(lastActiveFocus && !activeFocus)
      close()
    lastActiveFocus = activeFocus
  }
}
