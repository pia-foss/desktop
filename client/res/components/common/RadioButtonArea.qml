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

// RadioButtonArea is like ButtonArea but exposes a binary "checked" state in
// its screen reader annotations, and annotates as a radio button.
//
// Like CheckButtonArea, the clicked() action here really does have to toggle
// the state of the button, since screen readers will announce that as the
// action taken.
GenericButtonArea {
  id: buttonArea

  // name and description - Screen reader annotations, same as ButtonArea.
  property string name
  property string description
  // Whether the button is currently checked
  property bool checked

  NativeAcc.RadioButton.name: name
  NativeAcc.RadioButton.description: description
  NativeAcc.RadioButton.checked: checked
  NativeAcc.RadioButton.onActivated: mouseClicked()
}
