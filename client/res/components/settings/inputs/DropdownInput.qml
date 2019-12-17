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
  property bool indicatorButton: true

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
  function getIconForIndex(index) {
    var iconPath = model && model[index] && model[index].icon
    return iconPath || ""
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
    font: control.font
    visible: root.labelHeight > 0
  }

  ComboBox {
    id: control
    anchors.fill: parent
    anchors.topMargin: root.labelHeight
    // Pass just the names to the ComboBox as an array
    model: root.model.map(function(choice){return choice.name})
    // If the model provided to the ComboBox changes (such as due to a language
    // change), recalculate the current selection
    onModelChanged: control.currentIndex = mapValueToIndex(currentValue)

    implicitWidth: 200
    implicitHeight: 24

    NativeAcc.DropDownButton.name: label
    NativeAcc.DropDownButton.value: displayText
    NativeAcc.DropDownButton.onActivated: {
      popup.open()
    }

    enabled: root.enabled
    font.pixelSize: 13

    property real unhighlightedItemOpacity: 1.0

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

    // Like GenericButtonArea, button key activation has to occur on the
    // 'release' to ensure that the first menu item doesn't immediately activate
    // and close the menu.
    property var inKeyPress: null

    // Reimplement up/down and letter keys to work with disabled selections -
    // find the next viable selection.
    // Note that we have to handle the arrows in Keys.onPressed(), not
    // Keys.on[Up|Down]Pressed(), because we want ComboBox to handle this when
    // the popup is open, and this event isn't generated if something is
    // attached to on[Up|Down]Pressed() (even if they do not accept the event)
    Keys.onPressed: {
      // Let the ComboBox handle the selection if the popup is open
      if(popup.visible)
        return

      // Qt handles 'space' but it wouldn't show the focus cue; it doesn't
      // handle Enter/Return/Alt+Down/F4 at all.
      // Note that Alt+Down is particularly important because it appears in
      // screen reader documentation (NVDA, MS Narrator), even though it's not
      // actually a screen reader feature.

      // Button keys (space/enter/return) begin a key press.
      if(KeyUtil.handleButtonKeyEvent(event)) {
        focusCue.reveal()
        inKeyPress = event.key
        return
      }

      // Drop-down specific keys drop immediately.  This is actually different
      // from space/enter/return behavior on most platforms, though it varies
      // somewhat.
      if(KeyUtil.handleDropDownKeyEvent(event)) {
        focusCue.reveal()
        popup.open()
        return
      }

      var nextIndex = KeyUtil.handleVertKeyEvent(event, root.model, 'name',
                                                 currentIndex)
      if(nextIndex !== -1) {
        selectIndex(nextIndex)
        focusCue.reveal()
      }
    }

    Keys.onReleased: {
      if(inKeyPress && inKeyPress === event.key) {
        event.accepted = true
        inKeyPress = null
        popup.open()
      }
    }

    onActiveFocusChanged: {
      if(!activeFocus)
        inKeyPress = null
    }

    onActivated: selectIndex(index)

    delegate: ItemDelegate {
      id: choiceDelegate

      width: control.width
      height: 22

      NativeAcc.DropDownMenuItem.name: itemText.text
      NativeAcc.DropDownMenuItem.checked: control.currentIndex === index
      NativeAcc.DropDownMenuItem.highlighted: highlighted
      NativeAcc.DropDownMenuItem.onActivated: clicked()

      // 'disabled' is a role of the ListModel; it's not set for all models
      // and the default state is enabled.
      enabled: root.model[index] && !root.model[index].disabled

      highlighted: control.highlightedIndex === index

      contentItem: Rectangle {
        color: highlighted ? Theme.settings.inputDropdownSelectedColor : "transparent"
        anchors.fill: parent

        property bool hasIcon: !!iconPath
        property string iconPath: getIconForIndex(index)

        Item {
          visible: parent.hasIcon
          x: 5
          y: 0
          width: 20
          height: parent.height

          Image {
            anchors.centerIn: parent
            source: parent.parent.iconPath
            width: 20
            height: Qt.platform.os === 'osx' ? 15 : 16
            fillMode: Image.PreserveAspectFit
          }
        }
        Text {
          x: parent.hasIcon ? 25 : 0
          id: itemText
          leftPadding: 6
          rightPadding: 6
          anchors.verticalCenter: parent.verticalCenter
          width: parent.width
          text: modelData
          color: (root.enabled && choiceDelegate.enabled) ? Theme.settings.inputDropdownTextColor : Theme.settings.inputDropdownTextDisabledColor
          font: control.font
          elide: Text.ElideRight
          // Set this explicitly so "Arabic" correctly left-aligns when using an
          // LTR language (and, in theory, so everything else right-aligns when
          // using Arabic).
          horizontalAlignment: Text.AlignLeft
          verticalAlignment: Text.AlignVCenter
          opacity: highlighted ? 1.0 : control.unhighlightedItemOpacity
        }
      }
    }

    // The button at the right of the ComboBox
    indicator: Item {
      id: indicator
      anchors.right: control.right
      anchors.top: control.top
      anchors.bottom: control.bottom
      width: 17

      // The button frame is rounded on the right side and square on the left
      // side.  To do this, draw a rounded rectangle and a square rectangle, but
      // clip the rounded one on the right side of the button and the square one
      // on the left side.
      //
      // At scale factors >100%, the two rectangles' edges do not line up
      // perfectly - clip the two right at the edge of the rounded corner so
      // this isn't noticeable (just looks like the transition from the rounded
      // to straight edge).

      // Right side - rounded
      // Draw the button frame if 'indicatorButton' is true
      Item {
        id: roundedIndicatorClip
        visible: root.indicatorButton
        clip: true
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: background.radius

        Rectangle {
          id: roundedIndicator
          // 'anchors.fill: indicator' is really what we want, but QML cannot
          // anchor an item to an item that isn't an immediate parent/sibling
          anchors.right: parent.right
          anchors.top: parent.top
          anchors.bottom: parent.bottom
          width: indicator.width
          color: root.enabled ? Theme.settings.inputDropdownArrowBackgroundColor : Theme.settings.inputDropdownArrowDisabledBackgroundColor
          border.color: root.enabled ? Theme.settings.inputDropdownArrowBorderColor : Theme.settings.inputDropdownArrowDisabledBorderColor
          border.width: background.border.width
          radius: background.radius
        }
      }

      Item {
        visible: root.indicatorButton
        clip: true
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width - roundedIndicatorClip.width + 1 // Fudge by 1 due to rounding error at non-integer scale

        Rectangle {
          // again - would be 'anchors.fill: indicator' if QML supported it
          anchors.left: parent.left
          anchors.top: parent.top
          anchors.bottom: parent.bottom
          width: indicator.width
          color: roundedIndicator.color
          border.color: roundedIndicator.border.color
          border.width: roundedIndicator.border.width
        }
      }

      // Draw the arrow indicator
      Image {
        anchors.centerIn: parent
        source: root.enabled ? Theme.settings.inputDropdownArrowImage : Theme.settings.inputDropdownArrowDisabledImage
        width: 17
        height: 24
      }
    }

    contentItem: Item {
      property bool hasIcon: !!iconPath
      property string iconPath: getIconForIndex(control.currentIndex)

      Item {
        visible: parent.hasIcon
        x: 5
        y: 0
        width: 20
        height: parent.height

        Image {
          anchors.centerIn: parent
          source: parent.parent.iconPath
          width: 20
          height: Qt.platform.os === 'osx' ? 15 : 16
          fillMode: Image.PreserveAspectFit
        }
      }

      Text {
        leftPadding: 7
        x: parent.hasIcon ? 25 : 0
        rightPadding: control.indicator.width + control.spacing + x
        width: control.width
        anchors.verticalCenter: parent.verticalCenter
        text: control.displayText
        font: control.font

        color: root.enabled ? Theme.settings.inputDropdownTextColor : Theme.settings.inputDropdownTextDisabledColor
        // As in the delegate, explicitly align left so all languages align the
        // same way.
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
      }
    }

    background: Item {
      RectangularGlow {
        x: 0
        y: 1
        width: background.width
        height: background.height
        cornerRadius: background.radius
        glowRadius: 1
        color: "#33000000"
      }
      Rectangle {
        id: background
        width: parent.width
        height: parent.height
        border.color: Theme.settings.inputDropdownBorderColor
        border.width: control.visualFocus ? 2 : 1
        color: Theme.settings.inputDropdownBackgroundColor
        radius: 3
      }
    }

    popup: Popup {
      id: popup

      // The selected index when the popup is opened determines the Y coordinate
      // of the popup (to align the selected item with the combo box itself)
      property int popupShowInitialIndex: 0
      x: Util.popupXBindingFixup(popup, popup.parent.Window.window,
                                 popup.parent.Overlay.overlay, 0)
      y: Util.popupYBindingFixup(popup, popup.parent.Window.window,
                                 popup.parent.Overlay.overlay,
                                 -popupShowInitialIndex * 22)
      padding: 1

      contentItem: ListView {
        id: popupContent
        clip: true
        implicitHeight: contentHeight
        model: control.popup.visible ? control.delegateModel : null
        currentIndex: control.highlightedIndex
        boundsBehavior: Flickable.StopAtBounds

        // Like ThemedMenu, we don't annotate the whole popup menu itself;
        // VoiceOver doesn't like that annotation and works better with just the
        // items annotated.

        ScrollIndicator.vertical: ScrollIndicator {
        }
      }

      background: Rectangle {
        id: popupBackground
        implicitWidth: control.width
        implicitHeight: popupContent.implicitHeight + 2
        border.color: Theme.settings.inputDropdownBorderColor
        color: Theme.settings.inputDropdownBackgroundColor
        radius: 3
      }

      readonly property real popupActualY: {
        // Work around buggy Popup coordinate calculations
        var actualPos = Util.calculatePopupActualPos(popup, popup.parent.Window.window,
                                                     popup.parent.Overlay.overlay)
        return actualPos.y
      }

      enter: Transition {
        SequentialAnimation {
          PropertyAction { target: popupContent; property: "opacity"; value: 0.0 }
          PropertyAction { target: popupContent; property: "enabled"; value: true }
          PropertyAction { target: control; property: "unhighlightedItemOpacity"; value: 1.0 }
          ParallelAnimation {
            NumberAnimation { target: popupBackground; property: "y"; from: -popup.popupActualY; to: 0; duration: 150 }
            NumberAnimation { target: popupBackground; property: "height"; from: 24; to: popupContent.implicitHeight + 2; duration: 150 }
            NumberAnimation { target: popupBackground; property: "opacity"; from: 0.0; to: 1.0; duration: 150 }
          }
          NumberAnimation { target: popupContent; property: "opacity"; from: 0.0; to: 1.0; duration: 150 }
        }
      }
      exit: Transition {
        SequentialAnimation {
          PropertyAction { target: popupContent; property: "enabled"; value: false }
          NumberAnimation { target: control; property: "unhighlightedItemOpacity"; from: 1.0; to: 0.0; duration: 100 }
          NumberAnimation { target: popupContent; property: "opacity"; to: 0.0; duration: 100 }
          ParallelAnimation {
            NumberAnimation { target: popupBackground; property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InQuad }
            NumberAnimation { target: popupBackground; property: "y"; from: 0; to: -popup.popupActualY; duration: 200 }
            NumberAnimation { target: popupBackground; property: "height"; from: popupContent.implicitHeight + 2; to: 24; duration: 200 }
          }
        }
      }
      onAboutToShow: { popupShowInitialIndex = control.currentIndex }
      onClosed: control.forceActiveFocus(Qt.PopupFocusReason)
    }
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
