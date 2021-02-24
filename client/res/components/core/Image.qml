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

import QtQuick 2.11 as QtQuick
import QtQuick.Window 2.10 as QtQuickWindow
import "../client"

// Image tweaks:
//  - Not flipped by default for RTL; see rtlMirror.
QtQuick.Image {
  id: coreImage
  // By default, Image is not flipped for RTL.
  // Some images should flip though (directional arrows, etc.) - set rtlMirror
  // to true for these.
  //
  // The choice of default is a tough call; either way we rely on specific
  // testing for RTL to make sure images are displayed correctly.  It looks like
  // most images in the client do _not_ flip, so that's the default.
  property bool rtlMirror: false
  readonly property real rtlFlip: QtQuickWindow.Window.window ? QtQuickWindow.Window.window.rtlFlip : 1
  transform: [
    QtQuick.Scale {
      origin.x: width/2
      // If rtlMirror is true, do nothing here so the window's RTL flip applies
      // normally.
      // If rtlMirror is false, apply a flip here in RTL to cancel out the
      // window's RTL flip.
      xScale: rtlMirror ? 1 : coreImage.rtlFlip
    }
  ]
}
