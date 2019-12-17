// Copyright (c) 2019 London Trust Media Incorporated
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

import QtQuick 2.11 as QtQuick
import QtQuick.Controls 2.4 as QtQuickControls
import QtQuick.Window 2.10 as QtQuickWindow
import "../client"

// TextField tweaks:
// - Don't mirror for RTL (preserves both the rendered text and the caret
//   position)
QtQuickControls.TextField {
  id: coreTextField

  readonly property real rtlFlip: QtQuickWindow.Window.window ? QtQuickWindow.Window.window.rtlFlip : 1
  transform: [
    QtQuick.Scale {
      origin.x: width/2
      xScale: coreTextField.rtlFlip
    }
  ]
}
