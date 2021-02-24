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
import QtQuick.Controls 2.4
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

  // By default, the desired logical size is the content size.  These can be
  // overridden with lower values if the content is expected to be large.
  windowLogicalWidth: contentLogicalWidth
  windowLogicalHeight: contentLogicalHeight

  // The logical size of the content for this window - used to set up the scroll
  // view.
  property real contentLogicalWidth
  property real contentLogicalHeight

  // Items inside the window go in the inside the scroll view
  default property alias scrolledContent: scrollWrapper.windowScrollViewContent

  WindowScrollView {
    id: scrollWrapper
    anchors.fill: parent
    contentWidth: contentLogicalWidth
    contentHeight: contentLogicalHeight
    label: window.title
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
