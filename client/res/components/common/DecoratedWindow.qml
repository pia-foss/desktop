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

import QtQuick 2.0
import QtQuick.Window 2.10
import "../core"
import "../theme"
import PIA.NativeHelpers 1.0

// Functionality common to all windows in the client that have window
// decorations (all secondary modeless dialogs as well as the windowed
// dashboard).
Window {
  id: decoratedWindow

  // Replacement for show() that additionally positions the window and raises it,
  // which is normally what you want for a secondary modeless dialog window.
  function open() {
    // If the window is not already visible, reposition it with sane defaults.
    if (!visible) {
      // Going to position the window based on its size, the scale and size must
      // be initialized before we do this (needed for Linux)
      NativeHelpers.initScaling()
      centerOnActiveScreen();

      // If the window isn't visible already and doesn't have a focused control,
      // focus the first focusable control.  (Keep the existing focus if there
      // is a focused control.)
      if(!activeFocusItem) {
        var firstFocus = contentItem.nextItemInFocusChain(true)
        if(firstFocus)
          firstFocus.forceActiveFocus(Qt.ActiveWindowFocusReason)
      }
    }
    // Open and/or make the window visible.
    show();
    // Make the window the active window; raise() accomplishes this on macOS
    // whereas requestActivate() does the trick on Windows.
    raise();
    requestActivate();
  }

  function getScreenContainingMouseCursor() {
    var pt = NativeHelpers.getMouseCursorPosition();
    var screens = Qt.application.screens;
    for (var i = 0; i < screens.length; i++) {
      var s = screens[i];
      if (pt.x >= s.virtualX && pt.x < s.virtualX + s.width && pt.y >= s.virtualY && pt.y < s.virtualY + s.height) {
        return s;
      }
    }
    return screens[0];
  }

  // Position the window to appear in the center of the "active" screen.
  function centerOnActiveScreen() {
    // Use the same screen as the dashboard if the dashboard is visible,
    // otherwise use whichever screen contains the mouse cursor.
    //
    // The 'dashboard' property comes from main.qml; it doesn't exist when this
    // is used by the windowed 'splash' dashboard (on Linux when the daemon is
    // not running).
    if (typeof(dashboard) !== 'undefined' && dashboard.window && dashboard.window.visible && dashboard.window.screen) {
      screen = dashboard.window.screen;
    } else {
      screen = getScreenContainingMouseCursor();
    }
    // Center the window on the selected screen.
    x = Screen.virtualX + (Screen.width - width) / 2;
    y = Screen.virtualY + (Screen.height - height) / 2;
  }

  Component.onCompleted: {
    NativeHelpers.initDecoratedWindow(decoratedWindow)
  }
}
