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

import QtQuick 2.10
import Qt.labs.platform 1.1

// This menu bar is only loaded on Mac (by main.qml).
//
// We don't normally show a menu bar on Mac due to LSUIElement, but Qt still
// creates a menu bar with a Quit item by default, which enables the Cmd+Q
// shortcut to quit the app.
//
// This is easy to do accidentally and doesn't provide any feedback that it's
// occurring, so we disable this shortcut by creating a disabled Quit menu item.
MenuBar {
  Menu {
    title: "App"
    MenuItem {
      enabled: false
      role:  MenuItem.QuitRole
      text: "Quit"
    }
  }
}
