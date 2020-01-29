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
import QtQuick.Window 2.3
import "../../core"
import PIA.NativeHelpers 1.0

Item {
  id: appIcon

  property string appPath

  clip: logoImg.zoom !== 1

  readonly property real windowScale: Window.window ? Window.window.contentScale : 1.0

  Image {
    id: logoImg
    anchors.centerIn: parent
    readonly property real zoom: {
      // Windows UWP apps provide a logo that's intended to
      // cover a Start Menu tile - these usually have huge
      // margins.  The taskbar icon might be a better fit, but
      // there doesn't seem to be any API to get it.
      //
      // Zoom the icons to compensate.  This might crop full-
      // bleed icons, but this still seems to be better than a
      // bunch of tiny illegible icons.
      if(Qt.platform.os == 'windows' && appPath.startsWith('uwp:'))
        return 2.5
      return 1
    }
    width: parent.width * zoom
    height: parent.height * zoom
    source: "image://appicon/" + NativeHelpers.encodeUriComponent(appPath)

    // Set the source size to ensure the image is loaded in high
    // DPI if needed.  Qt applies its scale factor on Mac; on
    // Windows/Linux apply our own scale factor explicitly.
    sourceSize.width: width * appIcon.windowScale * zoom
    sourceSize.height: height * appIcon.windowScale * zoom
  }
}
