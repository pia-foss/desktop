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
import QtQuick.Controls 2.4
import PIA.FocusCue 1.0
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil

// WindowScrollView is a scroll view tuned for content that takes up all or most
// of a window.  It has focus cue and tabstop behavior appropriate for that use.
//
// The contentWidth and contentHeight _must_ be bound to the logical size of the
// content (per ThemedScrollView).  Bind label also for accessibility.
//
// This is the most general scroll view usable in many parts of the application,
// but ThemedScrollView is still used directly in several contexts so the
// accessibility behavior can be adjusted.
Item {
  property alias contentWidth: scrollView.contentWidth
  property alias contentHeight: scrollView.contentHeight
  property alias label: scrollView.label

  default property alias windowScrollViewContent: scrollWrapperFlickable.flickableData

  ThemedScrollView {
    id: scrollView
    anchors.fill: parent

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
        Util.ensureScrollViewBoundVisible(scrollView,
                                          scrollView.ScrollBar.horizontal,
                                          scrollView.ScrollBar.vertical,
                                          cueBound)
      }

      Keys.onPressed: {
        KeyUtil.handleScrollKeyEvent(event, scrollView,
          scrollView.ScrollBar.horizontal,
          scrollView.ScrollBar.vertical, scrollFocusCue)
      }
    }
  }

  // The focus cue for the scroll bars is outside of the scroll view, it
  // surrounds the visible part of the view.
  OutlineFocusCue {
    id: scrollFocusCue
    anchors.fill: parent
    control: scrollWrapperFlickable
    inside: true
  }
}
