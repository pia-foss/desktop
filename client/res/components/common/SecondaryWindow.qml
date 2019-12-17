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

import QtQuick 2.9
import QtQuick.Controls 2.4
import PIA.WindowScaler 1.0
import PIA.WindowMaxSize 1.0
import PIA.FocusCue 1.0
import "../client"
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil

// Functionality for a modeless dialog window with fixed-size content.
// Scales the content appropriately for the screen that it's on.  If the
// content does not fit on the screen, provides scroll bars and sizes to the
// largest size that will fit.
DecoratedWindow {
  id: window

  // The desired logical size of the window.  Same as the content size by
  // default, but can be smaller if the content is expected to be large.
  property real windowLogicalWidth: contentLogicalWidth
  property real windowLogicalHeight: contentLogicalHeight

  // The logical size of the content for this window
  property real contentLogicalWidth
  property real contentLogicalHeight

  // Whether the window should be resizeable.
  // If the window is resizeable, changes in windowLogical[Width|Height] will
  // resize it back to that size.  If these are bound, the bindings must not be
  // re-evaluated spuriously.  (Constants are OK, any calculated values might
  // have issues.)
  property bool resizeable: false

  // The actual logical size of the window
  readonly property real actualLogicalWidth: width / contentScale
  readonly property real actualLogicalHeight: height / contentScale

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

  // Items inside the window go in the Flickable inside the scroll wrapper
  default property alias scaledContent: scrollWrapperFlickable.flickableData

  flags: {
    // Both sizeable and fixed-size windows get minimize and close buttons.
    var flags = Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowMinimizeButtonHint | Qt.WindowCloseButtonHint
    // Resizeable windows also get a maximize button.  On Windows, fixed-size
    // windows need a fixed-size hint.
    if(resizeable)
      flags |= Qt.WindowMaximizeButtonHint
    else
      flags |= Qt.MSWindowsFixedSizeDialogHint
    return flags
  }

  WindowMaxSize {
    id: maxSize
    window: window
    preferredSize: Qt.size(windowLogicalWidth, windowLogicalHeight)
  }
  WindowScaler {
    id: scaler
    targetWindow: window
    logicalSize: maxSize.effectiveSize
    onCloseClicked: window.hide()
  }

  // Overlay scale and RTL flip
  Overlay.overlay.transform: [
    Scale {
      origin.x: scaleWrapper.width/2
      xScale: window.rtlFlip
    },
    Scale {
      xScale: contentScale
      yScale: contentScale
    }
  ]

  // Scale the content of the window, including the scroll wrapper and its
  // scroll bars.
  Item {
    id: scaleWrapper
    x: 0
    y: 0
    width: actualLogicalWidth
    height: actualLogicalHeight
    // Apply both the RTL flip and the scale factor to the scale wrapper
    transform: [
      Scale {
        origin.x: scaleWrapper.width/2
        xScale: window.rtlFlip
      },
      Scale {
        xScale: contentScale
        yScale: contentScale
      }
    ]

    ThemedScrollView {
      id: scrollWrapper
      anchors.fill: parent
      contentWidth: contentLogicalWidth
      contentHeight: contentLogicalHeight
      label: window.title

      Flickable {
        id: scrollWrapperFlickable
        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds

        // This is a tabstop only when scrolling is actually needed.  Most
        // secondary windows rarely need to scroll, this only happens when
        // none of the user's displays can show the whole window.
        //
        // Additionally, for some reason QQuickItem can't disable
        // activeFocusOnTab while the item is focused, so leave it enabled if
        // the item is focused right now.
        activeFocusOnTab: activeFocus || contentWidth > width || contentHeight > height

        FocusCue.onChildCueRevealed: {
          var cueBound = focusCue.mapToItem(scrollWrapperFlickable.contentItem,
                                            0, 0, focusCue.width,
                                            focusCue.height)
          Util.ensureScrollViewBoundVisible(scrollWrapper,
                                            scrollWrapper.ScrollBar.horizontal,
                                            scrollWrapper.ScrollBar.vertical,
                                            cueBound)
        }

        Keys.onPressed: {
          KeyUtil.handleScrollKeyEvent(event, scrollWrapper,
            scrollWrapper.ScrollBar.horizontal,
            scrollWrapper.ScrollBar.vertical, scrollFocusCue)
        }
      }
    }

    // The focus cue for the scroll bars is outside of the scroll view, it
    // surrounds the visible part of the view.  It's still inside the scale
    // wrapper so it's scaled correctly.
    OutlineFocusCue {
      id: scrollFocusCue
      anchors.fill: parent
      control: scrollWrapperFlickable
      inside: true
    }
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
}
