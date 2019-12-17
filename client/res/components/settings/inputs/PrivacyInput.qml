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
  property var itemList
  // Normally, the PrivacyInput sizes the items to minItemWidth.
  // If the largest item's size (which is the text width plus the minTextMargin)
  // is larger than minItemWidth, the privacy input grows to fit the items.
  // (This happens for several translations.)
  property int minTextMargin: 20
  property int minItemWidth: 70
  readonly property int itemWidth: {
    return Math.max(minItemWidth, itemRepeater.maxTextWidth + minTextMargin)
  }
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
  }

  InputDescription {
    x: 0
    id: descText
    anchors.top: mainLabel.bottom
    anchors.topMargin: 3
    text: desc
    width: Math.min(parent.width-infoTip.visibleWidth, implicitWidth)
  }

  Rectangle {
    id: itemWrapper
    anchors.topMargin: 7
    anchors.top: descText.bottom
    x: 0
    width: itemWidth * itemList.length
    height: itemHeight

    NativeAcc.Group.name: label

    color: Theme.settings.inputPrivacyBackgroundColor
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
      width: itemWidth
      color: enabled ? Theme.settings.inputPrivacySelectedBackgroundColor : Theme.settings.inputPrivacyDisabledBackgroundColor
      border.color: enabled ? Theme.settings.inputPrivacySelectedBorderColor : Theme.settings.inputPrivacyDisabledBorderColor
      opacity: 0.7
      radius: root.radius
      x: activeItem * itemWidth
      y: 0

      Behavior on x {
        SmoothedAnimation {
          duration: Theme.animation.quickDuration
        }
      }
    }

    Row {
      anchors.fill: parent
      Repeater {
        id: itemRepeater
        model: itemList

        readonly property int maxTextWidth: {
          // This depends on the repeater's children
          var dummyDep = itemRepeater.children
          var itemWidth = 0
          var item
          for(var i=0; i<itemRepeater.count; ++i) {
            item = itemRepeater.itemAt(i)
            if(item) {
              itemWidth = Math.max(itemWidth, item.textWidth)
            }
          }
          return itemWidth
        }
        Item {
          id: choiceButton
          height: itemHeight
          width: itemWidth

          NativeAcc.RadioButton.name: itemLabel.text
          NativeAcc.RadioButton.checked: activeItem === index
          NativeAcc.RadioButton.onActivated: choiceButton.mouseClicked()

          readonly property int textWidth: itemLabel.implicitWidth
          Text {
            id: itemLabel
            anchors.centerIn: parent
            z: 3
            color: Theme.settings.inputPrivacyTextColor
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

  InfoTip {
    id: infoTip
    anchors.left: itemWrapper.right
    anchors.verticalCenter: itemWrapper.verticalCenter
    anchors.leftMargin: 3

    property int visibleWidth: visible ? width : 0

    showBelow: true
    tipText: warning || info
    visible: warning || info
    icon: warning ? icons.warning : icons.settings
  }
}
