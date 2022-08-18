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

  // Whether the combo box is accessible (focusable with Tab and provides an
  // accessibility annotation), and its accessible name if so.  This is turned
  // off for table cells, since navigation and annotations are provided by the
  // table.  Other contexts should set a name so the control is accessible.
  property string accessibleName

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
  function getBackdropFlagForIndex(index) {
    var backdropFlag = model && model[index] && model[index].backdropFlag
    return !!backdropFlag;
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

    font.pixelSize: Theme.settings.inputLabelTextPx
    height: 50
    activeFocusOnTab: !!themedComboBox.accessibleName
    displayText: (model && model[currentIndex] && model[currentIndex].name) || ""

    NativeAcc.DropDownButton.name: themedComboBox.accessibleName
    NativeAcc.DropDownButton.value: displayText
    NativeAcc.DropDownButton.onActivated: themedComboBox.showPopup()

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
      height: themedComboBox.implicitHeight

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
        property bool hasBackdrop: !!modelData.backdropFlag
        property string iconPath: modelData.icon || ""

        Rectangle {
          opacity: 0.8
          visible: parent.hasIcon
          x: 0
          y: 0
          width: 40
          height: parent.height
          color: parent.hasBackdrop ? Theme.settings.inputDropdownIconBackdropColor : "transparent"

          Image {
            anchors.centerIn: parent
            source: parent.parent.iconPath
            width: 25
            height: Qt.platform.os === 'osx' ? 19 : 20
            fillMode: Image.PreserveAspectFit
          }
        }
        Text {
          x: parent.hasIcon ? 42 : 0
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
      anchors.rightMargin: 15
      width: 12      // Draw the arrow indicator

      Image {
        anchors.centerIn: parent
        source: Theme.dashboard.moduleExpandImage
        width: parent.width
        height: 7
      }
    }

    contentItem: Item {
      property bool hasIcon: !!iconPath
      property bool hasBackdrop: themedComboBox.getBackdropFlagForIndex(control.currentIndex)
      property string iconPath: themedComboBox.getIconForIndex(control.currentIndex)

      Rectangle {
        opacity: 0.8
        visible: parent.hasIcon
        x: 1
        y: 1
        width: 40
        height: parent.height - 2
        color: parent.hasBackdrop ? Theme.settings.inputDropdownIconBackdropColor : "transparent"
        Image {
          anchors.centerIn: parent
          source: parent.parent.iconPath
          width: 25
          height: Qt.platform.os === 'osx' ? 19 : 20
          fillMode: Image.PreserveAspectFit
        }
      }

      Text {
        leftPadding: 7
        x: parent.hasIcon ? 42 : 0
        rightPadding: 15 + control.indicator.width + control.spacing + x
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
      // of the popup (to align the selected item with the combo box itself).
      //
      // This can't cause the popup to extend outside the window; the fixup
      // functions apply the window edges and margins to determine the final
      // position.
      property int popupShowInitialIndex: 0
      x: Util.popupXBindingFixup(popup, popup.parent.Window.window,
                                 popup.parent.Overlay.overlay, 0)
      y: Util.popupYBindingFixup(popup, popup.parent.Window.window,
                                 popup.parent.Overlay.overlay,
                                 -popupShowInitialIndex * themedComboBox.implicitHeight)
      width: control.actualPopupWidth
      // Popup does not correctly limit apply the window size and margins when
      // the window scale is not 1.0 - see Util.popupYBindingFixup().  Since
      // this popup could be taller than the whole window (the Language popup
      // does this), we have to limit the height manually too.
      height: {
        let itemsHeight = 2*padding + control.count * themedComboBox.implicitHeight
        let window = popup.parent.Window.window
        let maxHeight = window ? window.actualLogicalHeight : 200
        maxHeight -= 2*margins
        return Math.min(itemsHeight, maxHeight)
      }
      padding: 1
      margins: 20

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

        ScrollBar.vertical: ScrollBar {
          policy: {
            let itemsHeight = 2*popup.padding + control.count * themedComboBox.implicitHeight
            let window = popup.parent.Window.window
            let maxHeight = window ? window.actualLogicalHeight : 200
            maxHeight -= 2*popup.margins

            return itemsHeight > maxHeight ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
          }
          contentItem: ThemedScrollBarContent {
          }
        }

        ScrollIndicator.vertical: ScrollIndicator {
        }
      }

      // Split up the actual background we draw (popupBackground) from the item
      // controlled by Popup (popupBackgroundWrapper).
      //
      // Popup really wants to be able to set size of the the background,
      // content, etc., whenever it wants - and in the case of a popup that has
      // to scroll due to exceeding the window height, it's hard to reliably get
      // the size that the background would be for our animation.  Splitting
      // these up gives Popup full control over the wrapper's position, and we
      // can animate the actual background inside it.
      background: Item {
        id: popupBackgroundWrapper
        Rectangle {
          id: popupBackground
          width: parent.width
          property real heightBlend: 0
          height: Util.mix(themedComboBox.implicitHeight,
            popupBackgroundWrapper.height, heightBlend)
          border.color: Theme.settings.inputDropdownBorderColor
          color: Theme.settings.inputDropdownBackgroundColor
          radius: 3
        }
      }

      readonly property real popupActualY: {
        // Work around buggy Popup coordinate calculations
        var actualPos = Util.calculatePopupActualPos(popup, popup.parent.Window.window,
                                                     popup.parent.Overlay.overlay)
        return actualPos.y
      }

      // Set the initial scroll position.  This only matters for a popup that
      // is larger than the window, since popups smaller than the window don't
      // have to scroll.
      function setInitialScrollPos() {
        // We want the initially selected index to be as close as possible to
        // the combo box control under the popup.  Figure out how many items
        // appear between the top edge of the popup and the top edge of the
        // combo box.
        let aboveItems = Math.round(-popup.popupActualY / themedComboBox.implicitHeight)
        // The top index will be an item preceding the current index, offset by
        // the number of items above the control.  A negative topIndex indicates
        // that one of the top few items is selected and we will be at the top
        // of the view, just use 0 in that case.
        let topIndex = control.currentIndex - aboveItems
        // The list view says that it will prevent us from scrolling beyond the
        // ends of the view, but avoid giving it invalid indices out of
        // paranoia
        if(topIndex < 0)
          popupContent.positionViewAtBeginning()
        else if(topIndex >= popupContent.count)
          popupContent.positionViewAtEnd()
        else
          popupContent.positionViewAtIndex(topIndex, ListView.Beginning)
        // It's rarely possible that if the window is so small that it scrolls,
        // the drop down could be outside of the popup - if the drop down is
        // just barely visible at the edge of the window, etc.  In that case,
        // we might have just placed the selected item outside of the view, so
        // ensure above all else that the selected item is visible.
        popupContent.positionViewAtIndex(control.currentIndex, ListView.Contain)
      }

      enter: Transition {
        SequentialAnimation {
          PropertyAction { target: popupContent; property: "opacity"; value: 0.0 }
          PropertyAction { target: popupContent; property: "enabled"; value: true }
          PropertyAction { target: control; property: "unhighlightedItemOpacity"; value: 1.0 }
          ScriptAction { script: popup.setInitialScrollPos() }
          ParallelAnimation {
            NumberAnimation { target: popupBackground; property: "y"; from: -popup.popupActualY; to: 0; duration: 150 }
            NumberAnimation { target: popupBackground; property: "heightBlend"; from: 0.0; to: 1.0; duration: 150 }
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
            NumberAnimation { target: popupBackground; property: "heightBlend"; from: 1.0; to: 0.0; duration: 200 }
          }
        }
      }
      onAboutToShow: { popupShowInitialIndex = control.currentIndex }
      onClosed: {
             // The popup has been closed, and the combo box has focused itself again
             // (unless another control already stole the focus - clicking another
             // control also dismisses the popup).
             //
             // However, for table cells, we don't want the control to be focused, we
             // want the table to get the focus.  It has already navigated the right
             // cell, so if the focused item is now our control (or nothing), invoke
             // focusOnDismissFunc() so the right table can be focused instead.
             //
             // For regular combo boxes outside of tables, we theoretically shouldn't
             // need to do anything, but there seems to be a bug in WindowAccImpl
             // preventing this focus change from being notified to the screen reader
             // correctly.  The default focusOnDismissFunc() just focuses the control
             // again which works around this.
             let focusItem = control && control.Window.window ? control.Window.window.activeFocusItem : null
             if(!focusItem || focusItem === control) {
               themedComboBox.focusOnDismissFunc()
             }
             // Otherwise, something else stole the focus; do not steal it back.
           }
      // Work around a crash in Qt - without this, the client would crash when
      // a combo box is destroyed while open.
      //
      // This appears to occur when:
      // - A ComboBox's Popup has an 'exit' transition
      // - That exit transition has at least one 'animation'
      // - The ComboBox is destroyed while open (such as the Split Tunnel app
      //   list receiving a change while a combo box is open, since the change
      //   causes the entire list to be rebuilt.)
      //
      // Normally it's difficult to cause that situation from the GUI client
      // alone (it _is_ possible if you click the combo boxes rapidly enough),
      // but it's easy to reproduce if you repeatedly change the
      // splitTunnelRules setting with piactl, and simultaneously attempt to
      // open one of the combo boxes.
      //
      // The crash occurs deep in Qt - the ComboBox is defocused as part of
      // destruction, which closes the popup, which triggers the exit
      // transition.  The exit transition is used in a call to
      // QtQml::qmlExecuteDeferred(), which attempts to find the
      // QQmlEnginePrivate for any deferred property bindings
      // (Transition.animations is deferred).  At this point,
      // data->context->engine is already nullptr, so it crashes.
      Component.onDestruction: popup.exit = null
    }
  }
}
