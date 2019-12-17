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
import PIA.NativeAcc 1.0 as NativeAcc

// ButtonArea is a MouseArea that supports keyboard navigation and screen
// reading hints, which are tuned for a MouseArea that acts like a button.
//
// ButtonArea is mostly a drop-in replacement for a MouseArea used as a button.
// A few additional properties are needed for screen reading.
//
// ButtonArea forwards some properties of MouseArea directly, such as
// acceptedButtons, hoverEnabled, etc.  Others, such as containsMouse and
// the clicked() signal, are hooked up to provide access via the keyboard as
// well.  See the properties below for details.
//
// There are no signals for other signals of MouseArea, like doubleClicked(),
// positionChanged(), etc.  If you need these signals, you should use a
// MouseArea and implement your own keyboard navigation, keeping in mind that
// hover/drag functionality needs to be reasonably accessible with the keyboard.
//
// Keep in mind that ButtonArea displays an OutlineFocusCue when focused, so its
// stacking order and positioning are important for that cue to appear properly.
GenericButtonArea {
  id: buttonArea

  // Screen reading properties.  These must be set for a ButtonArea, or it won't
  // be properly interpreted by screen readers.
  //
  // The name and description both describe the button.  Generally, the name is
  // the text displayed by the button.  Don't include the type of the control
  // (in either name or description), this is indicated with 'role'.
  //
  // Most buttons are fine with just a name.  If the name is not clear on its
  // own, or does not capture state that's expressed visually, the description
  // provides a slightly longer description of the button.
  property string name
  property string description

  NativeAcc.Button.name: name
  NativeAcc.Button.description: description
  NativeAcc.Button.onActivated: {
    // Simulate a click (including gaining mouse focus)
    mouseClicked()
  }
}
