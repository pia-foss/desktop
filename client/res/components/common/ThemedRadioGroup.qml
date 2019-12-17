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
import QtQuick.Layouts 1.3
import "qrc:/javascript/keyutil.js" as KeyUtil
import "../theme"
import "../core"
import PIA.NativeAcc 1.0 as NativeAcc

// ThemedRadioGroup creates a group of radio buttons.
// - When any button is selected by the user, indexSelected() and selected() are
//   emitted.
// - setSelectionIndex() and setSelection() can be used to change the selection.
//
// Each radio button can have an optional "action".  If specified, this creates
// a pushbutton next to the radio button (for configuration, etc.)
ColumnLayout {
  id: radioGroup
  spacing: 0

  // 'model' defines the choices in the ThemedRadioGroup.
  //
  // It is an array of objects with:
  // - name - the display string for the given choice
  // - value - the value used on setSelection() / selected()
  // - actionName - (optional) specifies name of the action button
  // - action - (optional)
  property var model

  // Set the current selection in the control.  -1 clears the selection (no
  // button is checked).
  function setSelectionIndex(index) {
    selectedIndex = index
  }

  // A new index has been selected by the user
  signal indexSelected(int index)

  // Set the current selection in the control by value.  'undefined' clears
  // the selection.
  function setSelection(value) {
    setSelectionIndex(model.findIndex(function(item){return item.value === value}))
  }

  // An item has been selected (provides 'value' from the model)
  signal selected(var value)

  // Selected index - stored separately from radio buttons' 'checked' state in
  // case the radio buttons are rebuilt
  property int selectedIndex

  // The radio buttons don't share a common parent, so group them manually
  ButtonGroup { id: autoGroup }

  Repeater {
    id: buttonRepeater
    model: radioGroup.model

    RowLayout {
      id: radioRow

      readonly property int choiceIndex: index
      readonly property var choice: modelData
      readonly property var radio: radioButton

      Layout.fillWidth: true
      spacing: 12

      RadioButton {
        id: radioButton
        text: radioRow.choice.name
        font.pixelSize: 13
        padding: 0
        ButtonGroup.group: autoGroup
        checked: radioRow.choiceIndex === radioGroup.selectedIndex

        NativeAcc.RadioButton.name: radioButton.text
        NativeAcc.RadioButton.checked: radioButton.checked
        NativeAcc.RadioButton.onActivated: radioButton.keyboardSelect()

        function handleSelected() {
          if (radioButton.checked) {
            radioGroup.selectedIndex = radioRow.choiceIndex
            radioGroup.indexSelected(radioRow.choiceIndex)
            radioGroup.selected(radioGroup.model[radioRow.choiceIndex].value)
          }
        }

        function keyboardSelect() {
          // Like CheckBox, RadioButton would normally toggle itself in the
          // key/mouse event handler.  toggle() manually here, but only if the
          // radio button is not checked (toggle() would uncheck the radio
          // button otherwise).
          if(!radioButton.checked)
            radioButton.toggle()
          radioButton.handleSelected()
        }

        onClicked: handleSelected()

        indicator: Image {
          id: indicator
          anchors.left: parent.left
          anchors.verticalCenter: parent.verticalCenter
          height: sourceSize.height/2
          width: sourceSize.height/2
          source: radioButton.checked ? Theme.settings.inputRadioOnImage : Theme.settings.inputRadioOffImage
        }

        contentItem: Text {
          id: text
          leftPadding: indicator.width + radioButton.spacing
          text: radioButton.text
          wrapMode: Text.WordWrap
          font: radioButton.font
          color: Theme.settings.inputLabelColor
          verticalAlignment: Text.AlignVCenter
        }

        OutlineFocusCue {
          id: radioButtonFocusCue
          anchors.fill: parent
          anchors.leftMargin: -4
          anchors.rightMargin: -4
          anchors.topMargin: 3
          anchors.bottomMargin: 3
          control: radioButton
        }

        Keys.onPressed: {
          if(KeyUtil.handlePartialButtonKeyEvent(event, focusCue))
            keyboardSelect()
        }
      }


      PushButton {
        id: radioAction

        visible: !!radioRow.choice.actionName
        height: 28

        minWidth: 75
        labels: [(radioRow.choice.actionName || "")]
        currentLabel: 0

        onClicked: {
          // For whatever reason, function properties aren't propagated through
          // to modelData, so obtain the original object from the model array
          // to invoke action()
          radioGroup.model[radioRow.choiceIndex].action()
        }
      }
    }
  }
}
