// Copyright (c) 2020 Private Internet Access, Inc.
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
import "../core"
import "../theme"
import PIA.NativeAcc 1.0 as NativeAcc

Item {
  id: themedComboBox

  // 'model' defines the choices displayed in the ThemedComboBox.
  // (It supports disabled items, unlike ComboBox.)
  // It's an array of objects with:
  // - name - the display string for the given choice
  // - disabled (optional) - prevents the choice from being selected when 'true'
  // - icon (optional) - path to the icon to display for this item
  //
  // (The last role is 'disabled' instead of 'enabled' so the default state is
  // enabled if the property is not given.)
  property alias model: control.model
  property alias currentIndex: control.currentIndex
  property alias displayText: control.displayText

  property bool focusOnTab: true

  // The popup can be allowed to extend to fit the items' text (often relevant
  // for French/Italian/Polish/Russian), but this isn't the default.
  property int popupMaxWidth: width

  // Manually show the popup (used to handle table keyboard and accessibility
  // events for drop-down cells)
  function showPopup() {
    // Focus the control, not the popup.  This is what Qt normally does in the
    // mouse click handler (the key events are handled by the control, even when
    // the popup is shown)
    control.forceActiveFocus(Qt.MouseFocusReason)
    control.popup.open()
  }

  // What to do to focus the control after the popup has been dismissed.  By
  // default, focuses the ComboBox itself.  (Overridden in tables to focus
  // the table cell.)
  property var focusOnDismissFunc: function() {
    control.forceActiveFocus(Qt.MouseFocusReason)
  }

  signal activated(int index)

  function getIconForIndex(index) {
    var iconPath = model && model[index] && model[index].icon
    return iconPath || ""
  }

  BorderImage {
    anchors.fill: control
    // Margin 5 with Y offset 1
    anchors.leftMargin: -5
    anchors.topMargin: -4
    anchors.rightMargin: -5
    anchors.bottomMargin: -6

    border {left: 10; top: 10; right: 10; bottom: 10}
    horizontalTileMode: BorderImage.Stretch
    verticalTileMode: BorderImage.Stretch
    source: Theme.settings.inputDropdownShadowImage
  }

  ComboBox {
    id: control
    anchors.fill: parent

    font.pixelSize: 13
    activeFocusOnTab: themedComboBox.focusOnTab
    displayText: (model && model[currentIndex] && model[currentIndex].name) || ""

    // Some translations have very long text that just can't fit in the
    // un-expanded combo box, so we expand the popup to show the full text
    // there.
    //
    // Ideally, we'd check the width of each item here and use the maximum
    // width to resize the popup, but this doesn't seem to be possible with the
    // ListModel, there seems to be no way to get all the way through the popup
    // from here to the instantiated delegates' properties.  This probably would
    // be better off using a Repeater in a Column instead.
    //
    // For now, since the only place this currently happens is the Name Servers
    // setting for Split Tunnel, and the first item is much longer, we just have
    // the first item report its width here whenever it resizes and use that.
    property int item0Width: 0

    readonly property int actualPopupWidth: {
      var w = width
      if(w < item0Width) {
        w = item0Width
      }
      if(w > themedComboBox.popupMaxWidth) {
        w = themedComboBox.popupMaxWidth;
      }
      return w
    }

    property real unhighlightedItemOpacity: 1.0

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

      var nextIndex = KeyUtil.handleVertKeyEvent(event, themedComboBox.model, 'name',
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

    onActivated: themedComboBox.activated(index)

    delegate: ItemDelegate {
      id: choiceDelegate

      width: control.actualPopupWidth
      height: 22

      NativeAcc.DropDownMenuItem.name: itemText.text
      NativeAcc.DropDownMenuItem.checked: control.currentIndex === index
      NativeAcc.DropDownMenuItem.highlighted: highlighted
      NativeAcc.DropDownMenuItem.onActivated: clicked()

      // 'disabled' is a role of the ListModel; it's not set for all models
      // and the default state is enabled.
      enabled: themedComboBox.model[index] && !themedComboBox.model[index].disabled

      highlighted: control.highlightedIndex === index

      contentItem: Rectangle {
        color: highlighted ? Theme.settings.inputDropdownSelectedColor : "transparent"
        anchors.fill: parent

        property bool hasIcon: !!iconPath
        property string iconPath: modelData.icon || ""

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
          text: modelData.name
          color: (control.enabled && choiceDelegate.enabled) ? Theme.settings.inputDropdownTextColor : Theme.settings.inputDropdownTextDisabledColor
          font: control.font
          elide: Text.ElideRight
          // Set this explicitly so "Arabic" correctly left-aligns when using an
          // LTR language (and, in theory, so everything else right-aligns when
          // using Arabic).
          horizontalAlignment: Text.AlignLeft
          verticalAlignment: Text.AlignVCenter
          opacity: highlighted ? 1.0 : control.unhighlightedItemOpacity

          // Hack property to report item 0's width back up to the popup, see
          // item0Width above
          readonly property int reportItem0Width: {
            var w = implicitWidth + x + choiceDelegate.leftPadding + choiceDelegate.rightPadding
            if(index === 0)
              control.item0Width = w
            return w
          }
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
      // Draw the button frame
      Item {
        id: roundedIndicatorClip
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
          color: control.enabled ? Theme.settings.inputDropdownArrowBackgroundColor : Theme.settings.inputDropdownArrowDisabledBackgroundColor
          border.color: control.enabled ? Theme.settings.inputDropdownArrowBorderColor : Theme.settings.inputDropdownArrowDisabledBorderColor
          border.width: background.border.width
          radius: background.radius
        }
      }

      Item {
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
        source: control.enabled ? Theme.settings.inputDropdownArrowImage : Theme.settings.inputDropdownArrowDisabledImage
        width: 17
        height: 24
      }
    }

    contentItem: Item {
      property bool hasIcon: !!iconPath
      property string iconPath: themedComboBox.getIconForIndex(control.currentIndex)

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

        color: control.enabled ? Theme.settings.inputDropdownTextColor : Theme.settings.inputDropdownTextDisabledColor
        // As in the delegate, explicitly align left so all languages align the
        // same way.
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
      }
    }

    background: Item {
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
      width: control.actualPopupWidth
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
      onClosed: themedComboBox.focusOnDismissFunc()
    }
  }
}
