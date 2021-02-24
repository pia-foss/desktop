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
import QtQuick.Controls 2.3
import PIA.NativeAcc 1.0 as NativeAcc

// ThemedScrollView is a ScrollView with:
// - themed scroll bar contents
// - activeFocusOnTab disabled (see below, handled by inner Flickable)
//
// You _must_ bind the contentWidth/contentHeight of the ThemedScrollView.  In
// Qt 5.12.1, the ScrollView does not seem to deduce the content size from a
// single item correctly.  (Just bind them to the item's implicit width/height
// usually.)
ScrollView {
  // Scale the scroll bars - needed in SettingsWindow since the scroll view is
  // outside the scale wrapper.
  property real scrollBarScale: 1.0
  // Label - used to set labels on scroll bars.  Usually the name of the window/
  // view that contains the scroll view.
  property string label

  // The scroll bars do not have any accessibility annotations.  Most apps do
  // not annotate these.  For blind users, they don't make any sense at all
  // since they don't affect navigation (the screen reader can navigate to
  // offscreen items).
  //
  // For low-vision users, we scroll automatically as the screen reader focuses
  // controls.  Adding the scroll bar element is mostly a nuisance, especially
  // with VoiceOver where it appears randomly in the middle of the control
  // order.  Users with motor impairments can use regular keyboard navigation to
  // scroll.
  ScrollBar.vertical.contentItem: ThemedScrollBarContent {
    scale: scrollBarScale
  }
  ScrollBar.horizontal.contentItem: ThemedScrollBarContent {
    scale: scrollBarScale
  }

  // In Qt 5.11, ScrollView seems to have activeFocusOnTab even though it's
  // a focus scope.  This breaks tab-focusing after wraparound, when the
  // scroll view is focused again, the last control is focused instead of
  // the first.
  // This is fixed in Qt 5.12, ScrollView is no longer a focus scope.
  // Explicitly setting activeFocusOnTab: false works around this and gives
  // consistent behavior on all Qt releases.  The inner Flickable handles
  // scrolling with the keyboard (which depends on context; some of the scroll
  // in one direction only while others scroll in both directions).
  activeFocusOnTab: false
}
