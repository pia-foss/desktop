// Copyright (c) 2024 Private Internet Access, Inc.
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
import "../../theme"
import "../../common"
import "../../core"
import PIA.NativeAcc 1.0 as NativeAcc

FocusScope {
  id: root

  property string label
  property string desc
  property string info
  property string warning
  // Accessibility text for the infoTip - overrides the warning/info, which are
  // used by default.  Use this if warning/info contains HTML markup, omit the
  // markup from the accessible text.
  property alias tipAccessibleText: infoTip.accessibleText
  property var itemList
  // PrivacyInput puts textMargin spacing on each side of each item
  property int textMargin: 20
  property int itemHeight: 25
  property int activeItem: 0
  property int radius: 20
  // Setting inputEnabled = false disables the control but leaves the InfoTip
  // enabled
  property alias inputEnabled: itemWrapper.enabled
  signal updated(int index);
  implicitHeight: itemWrapper.y + itemWrapper.height

  InputLabel {
    id: mainLabel
    x: 0
    y: 0
    text: label
    font.bold: false
  }

  InfoTip {
    id: infoTip
    anchors.left: mainLabel.right
    anchors.verticalCenter: mainLabel.verticalCenter
    anchors.leftMargin: 3

    property int visibleWidth: visible ? width : 0

    showBelow: true
    tipText: warning || info
    visible: warning || info
    icon: warning ? icons.warning : icons.settings
  }

  Rectangle {
    id: itemWrapper
    anchors.topMargin: 10
    anchors.top: mainLabel.bottom
    x: 0
    width: itemsRow.width
    height: itemsRow.height

    NativeAcc.Group.name: label

    color: Theme.settings.inputPrivacyBackgroundColor
    border.color: Theme.settings.inputDropdownBorderColor
    radius: root.radius

    // This Rectangle handles the keyboard interaction for a PrivacyInput,
    // mainly just because it makes sense for the focus cue to bound it
    activeFocusOnTab: true

    Keys.onPressed: {
      if(!enabled)
        return

      var nextIndex = KeyUtil.handleHorzKeyEvent(event, itemList, undefined,
                                                 activeItem)
      if(nextIndex !== -1) {
        if(nextIndex !== activeItem)
          updated(nextIndex)
        focusCue.reveal()
      }
    }

    Rectangle {
      height: itemHeight
      readonly property var activeItemElement: {
        let dep = itemRepeater.children
        let dep2 = itemRepeater.count
        return itemRepeater.itemAt(activeItem)
      }
      x: activeItemElement ? activeItemElement.x : 0
      y: 0
      width: activeItemElement ? activeItemElement.width : 10
      color: enabled ? Theme.settings.inputPrivacySelectedBackgroundColor : Theme.settings.inputPrivacyDisabledBackgroundColor
      border.color: enabled ? Theme.settings.inputPrivacySelectedBorderColor : Theme.settings.inputPrivacyDisabledBorderColor
      opacity: 1
      radius: root.radius

      Behavior on x {
        SmoothedAnimation {
          duration: Theme.animation.quickDuration
        }
      }
      Behavior on width {
        SmoothedAnimation {
          duration: Theme.animation.quickDuration
        }
      }
    }

    Row {
      id: itemsRow
      height: itemHeight
      Repeater {
        id: itemRepeater
        model: itemList

        Item {
          id: choiceButton
          height: itemHeight
          width: itemLabelUnselected.implicitWidth + 2*root.textMargin

          NativeAcc.RadioButton.name: itemLabelUnselected.text
          NativeAcc.RadioButton.checked: activeItem === index
          NativeAcc.RadioButton.onActivated: choiceButton.mouseClicked()

          // Applying "bold" to the selected item alters its width; use two
          // separate Texts so we can consistently use the "unselected" size to
          // size the element
          Text {
            id: itemLabelUnselected
            anchors.centerIn: parent
            visible: activeItem !== index
            color: Theme.settings.inputPrivacyTextColor
            text: modelData
          }
          Text {
            id: itemLabelSelected
            anchors.centerIn: parent
            visible: activeItem === index
            color: Theme.settings.inputPrivacySelectedTextColor
            font.bold: true
            text: modelData
          }

          function mouseClicked() {
            if(enabled) {
              itemWrapper.forceActiveFocus(Qt.MouseFocusReason)
              root.updated(index)
            }
          }

          MouseArea {
            anchors.fill: parent
            onClicked: choiceButton.mouseClicked()
          }
        }
      }
    }
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: itemWrapper
    control: itemWrapper
  }
}
