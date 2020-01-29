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

import QtQuick 2.11
import PIA.FocusCue 1.0
import "../theme"

Item {
  id: outlineFocusCue

  // The parent should bind the control property, and it should call the
  // 'reveal()' method if the user interacts with the control using the
  // keyboard.
  property alias control: focusCue.control
  property alias show: focusCue.show
  function reveal() {focusCue.reveal()}

  // By default the cue is drawn outside of the FocusCue bound, but it can be
  // drawn inside instead.
  property bool inside: false
  // Normally, a focus cue is rounded when outside, and square when inside.
  // Occasionally, an inside focus cue should still be rounded, set this to true
  // to force it.
  property bool forceRound: false

  // The color can be overridden - by default (when this is undefined), it
  // changes based on the theme.
  // In the header bar, it uses a different color when any dark header is shown.
  // This value should be a color or undefined.
  property var color: undefined

  // Margin applied to separate an 'outside' border from the control
  property real borderMargin: 2
  // Visual thickness of the border
  readonly property real visualBorderSize: Theme.popup.focusCueWidth
  // Total border size for an 'outside' border (not just the visual part,
  // includes the margin)
  readonly property real borderSize: borderMargin + visualBorderSize

  // The margin is applied to the FocusCue instead of just to the Rectangle, so
  // scrolling into view with FocusCue.onChildCueRevealed includes the margin.
  FocusCue {
    id: focusCue
    anchors.fill: parent
    anchors.margins: inside ? 0 : -borderSize

    Rectangle {
      anchors.fill: parent
      border.color: outlineFocusCue.color || Theme.popup.focusCueColor
      border.width: visualBorderSize
      color: "transparent"
      visible: parent.show
      radius: (inside && !forceRound) ? 0 : borderSize
    }
  }
}
