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

import QtQuick 2.9
import QtQuick.Window 2.10
import QtQuick.Controls 2.4
import "../core"
import "../theme"
import PIA.NativeHelpers 1.0
import PIA.WindowScaler 1.0
import PIA.WindowMaxSize 1.0

// Functionality common to all windows in the client that have window
// decorations (all secondary modeless dialogs as well as the windowed
// dashboard).
PiaWindow {
  id: decoratedWindow

  // Set these to the desired logical size of the window.
  property real windowLogicalWidth
  property real windowLogicalHeight

  // Whether the window should be resizeable.
  // If the window is resizeable, changes in windowLogical[Width|Height] will
  // resize it back to that size.  If these are bound, the bindings must not be
  // re-evaluated spuriously.  (Constants are OK, any calculated values might
  // have issues.)
  property bool resizeable: false


  // Whether this window needs to always be on top
  property bool onTop: false

  // The actual logical size of the window
  readonly property real actualLogicalWidth: maxSize.effectiveSize.width
  readonly property real actualLogicalHeight: maxSize.effectiveSize.height

  // Indicator that we're sizing and centering the window just before it's
  // shown.  Most PIA windows try to unload some or all of their content when
  // they're not being shown.  Just binding Loader.active to Window.visible
  // nearly works, _except_ when we need the content to determine the window's
  // size just before it's centered and shown.
  //
  // If the content is needed to determine the window size, load the content
  // when either Window.active or DecoratedWindow.positioningForShow is true.
  property bool positioningForShow: false

  // Properties provided by all top-level windows - see DashboardPopup

  // Content scale - needed for some overlay-layer components.
  // Although it's possible to scale Overlay.overlay itself, QML Popups have
  // issues with that:
  // - They don't apply the window edge limits correctly.  They're applied as if
  //   the window was not scaled, which can result in the item extending outside
  //   the edge of the window.
  // - The drop down animation isn't applied correctly (the expansion starts
  //   from the wrong place) - this might be an issue on our side, but there
  //   isn't anything obviously incorrect (and the first issue is a
  //   dealbreaker anyway).
  //
  // As a result, all popups have to scale manually using this contentScale
  // value.
  readonly property real contentScale: scaler.scale
  // Extra popup margins - needed for some popup components.  (Normally 0 for
  // decorated windows, can be overridden for specific windows.)
  property real popupAddMargin: 0

  flags: {
    // Both sizeable and fixed-size windows get minimize and close buttons.
    var flags = Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowMinimizeButtonHint | Qt.WindowCloseButtonHint
    // Resizeable windows also get a maximize button.  On Windows, fixed-size
    // windows need a fixed-size hint.
    if(resizeable)
      flags |= Qt.WindowMaximizeButtonHint
    else
      flags |= Qt.MSWindowsFixedSizeDialogHint

    if(onTop)
      flags |= Qt.WindowStaysOnTopHint
    return flags
  }

  WindowMaxSize {
    id: maxSize
    window: decoratedWindow
    preferredSize: Qt.size(windowLogicalWidth, windowLogicalHeight)
  }
  WindowScaler {
    id: scaler
    targetWindow: decoratedWindow
    logicalSize: maxSize.effectiveSize
    onCloseClicked: decoratedWindow.hide()
  }

  // Overlay scale and RTL flip
  Overlay.overlay.transform: [
    Scale {
      origin.x: scaleWrapper.width/2
      xScale: decoratedWindow.rtlFlip
    },
    Scale {
      xScale: contentScale
      yScale: contentScale
    }
  ]

  // Children of the DecoratedWindow are displayed in its contentItem.
  default property alias contents: contentItem.data

  Item {
    id: scaleWrapper
    x: 0
    y: 0
    width: actualLogicalWidth
    height: actualLogicalHeight
    // RTL flip and scale
    transform: [
      Scale {
        origin.x: scaleWrapper.width/2
        xScale: decoratedWindow.rtlFlip
      },
      Scale {
        xScale: contentScale
        yScale: contentScale
      }
    ]

    Item {
      z: 5
      id: contentItem
      anchors.fill: parent
    }
  }

  property var centerWindowTimer: Timer {
    interval: 0
    repeat: false
    running: false
    onTriggered: decoratedWindow.centerOnActiveScreen()
  }

  // Replacement for show() that additionally positions the window and raises it,
  // which is normally what you want for a secondary modeless dialog window.
  function open() {
    // If the window is not already visible, reposition it with sane defaults.
    if (!visible) {
      // Going to position the window based on its size, the scale and size must
      // be initialized before we do this (needed for Linux)
      NativeHelpers.initScaling()
      // If the content is loaded dynamically, tell the window that we're
      // centering so the content is loaded
      positioningForShow = true
      centerOnActiveScreen()

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
    // The window is now visible, we're done positioning for the initial show
    positioningForShow = false
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

  // Handle close keys.
  // Alt+F4 on Windows can be handled by the window procedure, but Cmd+W on Mac
  // is our responsibility.
  Shortcut {
    sequence: StandardKey.Close
    context: Qt.WindowShortcut
    onActivated: {
      // If the dashboard is visible, focus it before hiding (otherwise it would
      // hide since the application is losing focus)
      if(dashboard.window)
        dashboard.window.focusIfVisible()
      hide()
    }
  }

  Component.onCompleted: {
    NativeHelpers.initDecoratedWindow(decoratedWindow)
  }
}
