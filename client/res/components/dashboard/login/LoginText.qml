// Copyright (c) 2023 Private Internet Access, Inc.
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
import "../../theme"
import "../../common"

Item {
  id: loginText

  property bool errorState: false
  // Causes an 'error' info tip to be displayed in the control.  When active,
  // errorState has no effect, the control displays the error state due to the
  // error tip.
  property string errorTipText: ""
  property alias text: textField.text
  property alias placeholderText: textField.placeholderText
  property alias masked: textField.masked
  property alias inputMask: textField.inputMask
  property alias validator: textField.validator
  readonly property bool acceptableInput: textField.acceptableInput

  signal updated(string val);
  signal accepted();

  implicitHeight: 40

  Rectangle {
    color: {
      if (errorState || errorTipText) {
        return Theme.login.inputErrorAccentColor
      } else if (textField.focus) {
        return Theme.login.inputFocusAccentColor
      } else {
        return Theme.login.inputDefaultAccentColor
      }
    }

    height: 1
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
  }

  ThemedTextField {
    id: textField
    anchors.left: parent.left
    anchors.right: warningTip.left
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    palette.text: Theme.login.inputTextColor
    label: placeholderText

    background: Item{}

    onTextEdited: {
      loginText.updated(text)
    }
    onAccepted: loginText.accepted()
  }

  InfoTip {
    id: warningTip
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    width: visible ? implicitWidth : 0
    showBelow: false
    tipText: loginText.errorTipText
    visible: !!loginText.errorTipText
    icon: icons.warning
  }
}
