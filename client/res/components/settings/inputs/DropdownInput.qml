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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.11
import QtGraphicalEffects 1.0
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil
import "../stores"
import "../../common"
import "../../core"
import "../../theme"
import PIA.NativeAcc 1.0 as NativeAcc

FocusScope {
  id: root

  property Setting setting
  // 'model' defines the choices in the DropdownInput.
  // It's an array of objects with:
  // - name - the display string for the given choice
  // - value - the value stored in the setting for that choice
  // - disabled (optional) - prevents the choice from being selected when 'true'
  //
  // (The last role is 'disabled' instead of 'enabled' so the default state is
  // enabled if the property is not given.)
  property var model

  property bool enabled: true
  property string label: ""
  // Info / warning - display an Info Tip with the appropriate icon.  Only one
  // of these should be set at a time.
  property string info
  property string warning
  // Show the info tip popup below (true, default), or above (false)
  property bool tipBelow: true
  property alias popupMaxWidth: control.popupMaxWidth

  readonly property var currentValue: setting ? setting.currentValue : undefined
  readonly property int labelHeight: label === "" ? 0 : text.contentHeight + 5

  // activated - Emitted when a selection is made in the combo box after
  // updating setting.currentValue.  Only emitted if the current value actually
  // changes.
  signal activated

  width: parent.width;
  implicitWidth: control.implicitWidth
  implicitHeight: control.implicitHeight + labelHeight

  onCurrentValueChanged: control.currentIndex = mapValueToIndex(currentValue)

  function mapValueToIndex(value) {
    if (model && value !== undefined) {
      for (var i = 0; i < model.length; i++) {
        if (model[i].value === value) {
          return i;
        }
      }
      console.warn("couldn't find index for value", value);
    }
    return -1;
  }

  function mapIndexToValue(index) {
    if (model && index >= 0 && index < model.length) {
      return model[index].value;
    }
    return undefined;
  }

  LabelText {
    id: text
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.right: parent.right
    height: root.labelHeight

    text: label
    color: root.enabled ? Theme.settings.inputLabelColor : Theme.settings.inputLabelDisabledColor
    font.pixelSize: 13
    visible: root.labelHeight > 0
  }



  ThemedComboBox {
    id: control
    anchors.fill: parent
    anchors.topMargin: root.labelHeight
    model: root.model
    // If the model provided to the ComboBox changes (such as due to a language
    // change), recalculate the current selection
    onModelChanged: control.currentIndex = mapValueToIndex(currentValue)

    implicitWidth: 230
    implicitHeight: 24

    accessibleName: label

    enabled: root.enabled

    // Change the selection to `index` due to a user interaction (if possible).
    // This is specifically for user interaction, it's not appropriate for
    // selection changes due to the backend setting updating, etc.:
    // - it restores the existing selection of `index` can't be chosen
    // - it emits activated() if the selection changes
    function selectIndex(index) {
      var selectionModel = root.model[index]
      // Prevent selecting disabled items.
      // Most of the selection methods that could select disabled items are
      // overridden, but we rely on this for some more obscure methods, like
      // manually selecting an item from the dropdown
      if(!selectionModel || selectionModel.disabled) {
        // Can't select this item, restore the original value from the setting
        control.currentIndex = mapValueToIndex(currentValue)
      }
      // Otherwise, we can select this value, write it if it has actually changed
      else if (setting.currentValue !== selectionModel.value) {
        setting.currentValue = selectionModel.value
        root.activated()
      }
    }

    onActivated: selectIndex(index)
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: control
    control: control
  }

  InfoTip {
    id: infoTip
    anchors.left: control.right
    anchors.verticalCenter: control.verticalCenter
    anchors.leftMargin: 3
    showBelow: root.tipBelow
    tipText: warning || info
    visible: warning || info
    icon: warning ? icons.warning : icons.settings
  }

  Component.onCompleted: {
    control.currentIndex = mapValueToIndex(currentValue)
  }
}
