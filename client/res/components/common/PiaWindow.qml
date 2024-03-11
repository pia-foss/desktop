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

import QtQuick 2.0
import QtQuick.Window 2.10
import "../core"
import PIA.NativeHelpers 1.0
import PIA.WindowFormat 1.0

// PiaWindow contains behavior common to all windows in the PIA client - both
// decorated windows and the popup-mode dashboard.  (The popup-mode dashboard
// only shares a few behaviors with other windows; most common behavior is in
// DecoratedWindow, which applies to everything except the popup-mode
// dashboard.)
Window {
  id: piaWindow

  // On Mac only, the OpenGL context appears to be shared (?) even though we
  // didn't set the application attribute to indicate that.  As a result, the
  // first window shown determines whether we get transparency, and we need
  // transparency to render the popup mode dashboard correctly.
  //
  // We have to mark _all_ windows as transparent in order to ensure that we
  // get an alpha-blended OpenGL context regardless of which window is shown
  // first.  On Mac, this should have little performance impact since all window
  // buffers actually get alpha channels anyway.  This isn't needed on other
  // platforms, so default to false otherwise.
  //
  // Note that the popup dashboard overrides this with its own specific behavior
  // that's needed on Windows/Linux, but it always sets it to true on Mac so the
  // behavior is still correct.
  WindowFormat.hasAlpha: Qt.platform.os === 'osx'

  // If the window is hidden (from Cmd+W or the close button), check if we need
  // to deactivate the app on Mac.  If no window is focused, deactivate the app
  // to focus the next app.
  //
  // This is indicated by 'visible' changing to 'false', but we have to defer
  // the actual check until Qt has actually hidden the window, so we know
  // whether AppKit focused another window or not.
  Timer {
    id: checkDeactivateTimer
    interval: 1
    repeat: false
    running: false
    onTriggered: NativeHelpers.checkAppDeactivate()
  }
  // When the dashboard is shown/hidden, update UIState
  onVisibleChanged: {
    if(!visible)
      checkDeactivateTimer.start()
  }
}
