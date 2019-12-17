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
import QtQuick.Window 2.11
import "../theme"

Behavior {
  // Parent Item - needed to hook up the parent Window.  Behavior is not an
  // Item, so we don't get this implicitly.
  property Item parent

  OpacityAnimator {
    duration: Theme.animation.normalDuration
  }
  // If the dashboard is not visible, disable this behavior.
  //
  // For whatever reason, QML does not seem to process property changes while
  // the window is hidden, which means that if a state change occurs while the
  // window is hidden, the animation would trigger when it is next shown.  This
  // is strange if the dashboard is hidden in the connecting/disconnecting state
  // (yellow header), then shown in a different state - the yellow header
  // briefly appears and fades away.
  enabled: parent.Window.window.visible
}
