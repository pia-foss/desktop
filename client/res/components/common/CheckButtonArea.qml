// Copyright (c) 2022 Private Internet Access, Inc.
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

// CheckButtonArea is like ButtonArea but exposes a binary "checked" state in
// its screen reader annotations.
//
// CheckButtonArea emits its 'clicked' signal when the user wants to toggle it
// (from the mouse, keyboard, or a screen reader).  This really should toggle
// the state, since screen readers will state that this is the action taken.
// (Don't take some other action, like showing a prompt, etc.)
//
// It appears to be a check box to screen readers.  (It seems like the QML
// Accessible type would allow a Button to set a 'checkable' annotation, but
// this doesn't seem to do anything.)
GenericButtonArea {
  id: buttonArea

  // name and description - Screen reader annotations, same as ButtonArea.
  // The role for a CheckButtonArea is always 'CheckBox' and can't be changed.
  property string name
  property string description
  // Whether the button is currently checked
  property bool checked
  // SettingsToggleButton has to manually set the disabled accessibility hint,
  // it doesn't actually disable so it can still show the button's name when
  // pointed.
  property bool accDisabled: false

  NativeAcc.CheckButton.name: name
  NativeAcc.CheckButton.description: description
  NativeAcc.CheckButton.checked: checked
  NativeAcc.CheckButton.disabled: accDisabled
  NativeAcc.CheckButton.onActivated: mouseClicked()
}
