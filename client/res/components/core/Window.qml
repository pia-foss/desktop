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

import QtQuick 2.0 as QtQuick
import QtQuick.Window 2.10 as QtQuickWindow
import "../client"

// Window tweaks:
// - Add RTL flip properties that are used by other 'core' types to detect RTL
//   flip
QtQuickWindow.Window {
  // This property indicates whether the window is flipped for RTL.
  // All windows except the dev tools window flip based on the active language.
  // The dev tools window doesn't flip; it uses several QtQuick.Controls types
  // that can't be flipped properly (see DevToolsWindow)
  property bool rtlMirror: Client.state.activeLanguage.rtlMirror
  // Layout flip - +1 for LTR (non-mirrored) languages, -1 for RTL (mirrored)
  // languages.
  // (Used to apply mirror transformations for RTL.)
  readonly property real rtlFlip: rtlMirror ? -1 : 1
}
