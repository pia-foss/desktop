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
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0
import "../stores"
import "../../theme"
import "../../common"

FocusScope {
  id: root

  property Setting setting

  property bool enabled: true
  property string label: ""
  property alias validator: control.validator
  property alias placeholderText: control.placeholderText
  property alias masked: control.masked
  readonly property alias acceptableInput: control.acceptableInput
  readonly property string text: control.text

  readonly property var currentValue: setting ? setting.currentValue : undefined
  readonly property int labelHeight: label === "" ? 0 : text.contentHeight + 5

  signal accepted();

  width: parent.width;
  implicitWidth: control.implicitWidth
  implicitHeight: control.implicitHeight + labelHeight + textBoxVerticalPadding
  property int textBoxVerticalPadding: 0

  onCurrentValueChanged: control.text = currentValue

  LabelText {
    id: text
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.right: parent.right
    height: root.labelHeight
    text: label
    color: root.enabled ? Theme.settings.inputLabelColor : Theme.settings.inputLabelDisabledColor
    font: control.font
    visible: root.labelHeight > 0
  }

  SettingsTextField {
    textBoxVerticalPadding: parent.textBoxVerticalPadding
    id: control
    anchors.fill: parent
    anchors.topMargin: root.labelHeight
    enabled: root.enabled
    label: root.label

    borderColor: !control.acceptableInput && !control.activeFocus ?
      Theme.settings.inputTextboxInvalidBorderColor :
      control.defaultBorderColor

    onEditingFinished: {
      if (acceptableInput && setting.currentValue !== control.text) {
        setting.currentValue = control.text;
      }
    }
    onAccepted: root.accepted()
  }

  Component.onCompleted: {
    control.text = currentValue;
  }
}
