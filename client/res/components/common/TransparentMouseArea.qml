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
import QtQuick.Controls 2.3

// TransparentMouseArea is a MouseArea that propagates all events up to other
// MouseAreas higher in the stacking order.
//
// It's intended to be used when a specific element of a control should have a
// hover effect (like an InfoTip), but clicks are still passed through to a
// parent.
//
// Note that if the parent has hover effects, those should usually still be
// applied, and MouseArea does not offer a way to propagate hover events.  (The
// parent just has to check both MouseAreas' containsMouse flags.)
MouseArea {
  property bool propagateClicks: true
  property bool propagateWheel: true
  propagateComposedEvents: propagateClicks
  onClicked: mouse.accepted = !propagateClicks
  onDoubleClicked: mouse.accepted = !propagateClicks
  onPositionChanged: mouse.accepted = !propagateClicks
  onPressAndHold: mouse.accepted = !propagateClicks
  onPressed: mouse.accepted = !propagateClicks
  onReleased: mouse.accepted = !propagateClicks
  onWheel: wheel.accepted = !propagateWheel
}
