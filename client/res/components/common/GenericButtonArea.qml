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
import "qrc:/javascript/keyutil.js" as KeyUtil

// GenericButtonArea is a MouseArea that supports keyboard navigation tuned for
// a MouseArea that acts like a button.
//
// This is used to implement ButtonArea and CheckButtonArea.  Most of the time,
// use one of these instead, since they also provide accessibility annotations.
//
// GenericButtonArea could be used directly for unusual buttons that require
// custom screen reading annotations, but consider extracting that to a reusable
// ButtonArea-like component instead.
Item {
  id: buttonArea

  // Properties of the MouseArea that can be used directly.  ButtonArea can be
  // configured for any mouse buttons / cursor shape, like MouseArea.
  //
  // Most properties should work, more could be added here.  The MouseArea is
  // hidden mainly to prevent mistaken connections to its clicked() signal.
  // (Note that 'enabled' and 'visible' are also availble as properties from
  // Item that apply to its children by default.)
  property alias acceptedButtons: mouseArea.acceptedButtons
  property alias cursorShape: mouseArea.cursorShape
  property alias hoverEnabled: mouseArea.hoverEnabled

  // These properties are _not_ forwarded:
  // - drag, mouseX, mouseY, pressAndHoldInterval, pressedButtons,
  //   preventStealing, propagateComposedEvents, scrollGestureEnabled
  // Most of these don't make much sense for a ButtonArea, like dragging-related
  // properties.  There isn't a clear way to model those in general with the
  // keyboard.  If you need those, you should probably use a MouseArea directly
  // and implement custom keyboard navigation.

  // These properties model the corresponding ones of MouseArea:

  // containsPress is enabled for a real mouse button press, or for a key press.
  // It's still a shortcut for containsMouse && pressed.
  readonly property bool containsPress: containsMouse && pressed
  // containsMouse is enabled either when the area actually contains the mouse
  // _or_ when it is showing the keyboard focus cue.  This triggers hover
  // effects, which are sensible to show when focusing with the keyboard.
  //
  // Keep in mind that it's possible for more than one ButtonArea to have
  // containsMouse=true - one can be keyboard-focused while the other is
  // pointed.
  readonly property bool containsMouse: mouseArea.containsMouse || focusCue.show || inKeyPress
  // pressed is enabled either for a real mouse button press or for a key press.
  readonly property bool pressed: mouseArea.pressed || inKeyPress

  // clicked() is emitted when the ButtonArea is clicked _or_ when it is
  // activated with the keyboard.  It does _not_ provide a MouseEvent like
  // MouseArea, since it could originate from the keyboard.
  signal clicked()

  // There are no signals for double click, drag, hold, wheel, etc.  These don't
  // fit the ButtonArea model - if you need these, consider using a MouseArea
  // and providing custom keyboard navigation.
  //
  // entered() and exited() could be provided based on the combined
  // containsMouse state; these are not provided right now just because they
  // aren't used.

  // Some focus cue properties can be overridden, such as to change the color
  // (in the header) or put the border on the inside.
  property alias focusCueColor: focusCue.color
  property alias focusCueInside: focusCue.inside
  property alias focusCueForceRound: focusCue.forceRound
  property alias focusCueMargin: focusCue.borderMargin

  activeFocusOnTab: true

  // Accessibility event handlers can call mouseClicked() to simulate a click on
  // the button.
  function mouseClicked() {
    // Gain focus when clicked - this does not show the focus cue yet since it
    // is a mouse focus
    buttonArea.forceActiveFocus(Qt.MouseFocusReason)
    buttonArea.clicked()
  }

  MouseArea {
    id: mouseArea
    anchors.fill: parent
    onClicked: mouseClicked()
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: buttonArea
    control: buttonArea
  }

  // ButtonArea activates on key _release_ events, like most controls do on
  // most platforms (though this varies considerably).
  //
  // This is important for a few cases, such as the header menu button, because
  // activating on 'press' would allow the 'release' event to go to the first
  // menu item and immediately activate it.  (MenuItem isn't smart enough to
  // realize that it didn't get the corresponding 'press' event.)
  //
  // Additionally, we show 'press' effects while the key is down.
  //
  // inKeyPress is set to the key that was pressed when a press event occurs.
  // If it's still set when the key release event occurs, we activate the
  // button.
  property var inKeyPress: null

  Keys.onPressed: {
    if(KeyUtil.handleButtonKeyEvent(event)) {
      focusCue.reveal()
      inKeyPress = event.key
    }
  }
  Keys.onReleased: {
    if(inKeyPress && inKeyPress === event.key) {
      event.accepted = true
      inKeyPress = null
      clicked()
    }
  }
  // Loss of focus cancels any key press.
  onActiveFocusChanged: {
    if(!activeFocus)
      inKeyPress = null
  }
}
