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
import "../theme"
import PIA.NativeHelpers 1.0

// DashboardSlideAnimator animates some properties used to implement the
// dashboard's slide-and-fade hide/show animations.
//
// The only real reason for this is to have a State that causes some animated
// transitions to occur.  (`Window` isn't an Item and doesn't have states,
// although DashboardWindow needs more than one independent state anyway.)
Item {
  id: dashSlideAnimator
  // Set dashVisible to the desired state of the dashboard.  When it changes to
  // 'true', the show animation is triggered.  When it changes to 'false', the
  // hide animation is triggered.
  property bool dashVisibleState: false

  // Progress of the animation - used to compute the animation properties
  property real showProgress

  // Whether the dashboard should actually be visible based on the animation
  // state
  readonly property bool dashVisible: showProgress > 0.0
  // The dashboard's opacity
  readonly property real dashOpacity: showProgress
  // The position offset for the slide animation.  Ranges from 0 - 80;
  // DashboardWindowPlacement applies this in a direction based on the show
  // direction.
  readonly property real dashSlideOffset: (1.0 - showProgress) * Theme.animation.dashSlideAmount;

  states: [
    State {
      name: "shown"
      when: dashVisibleState
      PropertyChanges {
        target: dashSlideAnimator
        showProgress: 1.0
      }
    },
    State {
      name: "hidden"
      when: !dashVisibleState
      PropertyChanges {
        target: dashSlideAnimator
        showProgress: 0.0
      }
    }
  ]

  // Hiding the dashboard occurs with a 500ms animation on Windows only.
  // This is needed in order for a second click on the tray icon to hide the
  // dashboard correctly.  It doesn't fit well in OS X though, the dashboard
  // feels like it's in the way longer than necessary - most UI elements on OS X
  // hide instantly or with very fast animations.
  //
  // On Windows, the technical reason for this animation is that the tray itself
  // steals focus when a left-button-down occurs on the tray icon.  There is no
  // way to differentiate this from losing focus to some other app, so the
  // application hides then.  We're only told anything by the tray when the
  // left-button-up occurs - we get a notification then, but at that point we
  // have already hidden and the dashboard would re-show.
  //
  // Windows does this to provide drag-and-drop of tray icons with the left
  // mouse button.  (OS X does it with Cmd+drag to avoid this sort of nonsense.)
  //
  // This animation delays the actual hide for 500ms, so as long as the click
  // takes less than 500ms, we stay hidden correctly.  If the user holds the
  // button for longer than 500ms (without moving to start a drag), we hide and
  // then re-show when the button is released, which is suboptimal but not *too*
  // surprising.
  //
  // This animation cannot be much less than 500ms (which is why it's set
  // specifically instead of using Theme.animation.normalDuration).  Some quick
  // tests showed that my clicks usually last ~100-150ms, but occasionally they
  // are close to 350ms.  500ms leaves some headroom over that, some people are
  // probably slower clickers than me.  As a point of reference, the default
  // double-click time on Windows is also 500ms.
  transitions: [
    Transition {
      from: "shown"
      to: "hidden"
      enabled: NativeHelpers.platform === NativeHelpers.Windows
      NumberAnimation {
        id: hideAnimation
        target: dashSlideAnimator
        property: "showProgress"
        duration: 500
        easing.type: Easing.OutQuad
      }
    }
  ]
}
