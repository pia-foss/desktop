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

import QtQuick 2.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import QtQuick.Window 2.11
import "../common"
import "../core"
import "../theme"
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil
import PIA.NativeAcc 1.0 as NativeAcc

// This draws a themed dialog in the overlay layer of the current window.
// The window should be a SecondaryWindow; OverlayDialog uses its
// actualLogicalWidth and actualLogicalHeight properties to center itself
Dialog {

  // The buttons at the bottom of the dialog. An array of either standard buttons
  // (see Dialog constants), or objects with the following
  property var buttons

  // Boolean properties to enable/disable categories of buttons
  property bool canAccept: true
  property bool canReject: true
  property bool canDiscard: true
  property bool canYes: true
  property bool canNo: true
  property bool canReset: true
  property bool canApply: true

  // Signal when a dialog button is clicked. The argument is a button
  // descriptor object of the same format that 'button' accepts, with
  // the following key fields:
  //
  // - index: the index of the button in the 'buttons' array
  // - role: the button role (e.g. DialogButtonBox.AcceptRole)
  // - code: the standard button code if present (e.g. Dialog.Ok)
  //
  signal clicked(var button)

  readonly property var defaultButtons: {
    var dict = {};
    dict[Dialog.Ok] = { text: uiTr("OK", "dialog button"), role: DialogButtonBox.AcceptRole };
    dict[Dialog.Open] = { text: uiTr("Cancel", "dialog button"), role: DialogButtonBox.AcceptRole };
    dict[Dialog.Save] = { text: uiTr("Save", "dialog button"), role: DialogButtonBox.AcceptRole };
    dict[Dialog.Cancel] = { text: uiTr("Cancel", "dialog button"), role: DialogButtonBox.RejectRole };
    dict[Dialog.Close] = { text: uiTr("Close", "dialog button"), role: DialogButtonBox.RejectRole };
    dict[Dialog.Discard] = { text: uiTr("Discard", "dialog button"), role: DialogButtonBox.DestructiveRole };
    dict[Dialog.Apply] = { text: uiTr("Apply", "dialog button"), role: DialogButtonBox.ApplyRole };
    dict[Dialog.Reset] = { text: uiTr("Reset", "dialog button"), role: DialogButtonBox.ResetRole };
    dict[Dialog.RestoreDefaults] = { text: uiTr("Restore Defaults", "dialog button"), role: DialogButtonBox.ResetRole };
    dict[Dialog.Help] = { text: uiTr("Help", "dialog button"), role: DialogButtonBox.HelpRole };
    dict[Dialog.SaveAll] = { text: uiTr("Save All", "dialog button"), role: DialogButtonBox.AcceptRole };
    dict[Dialog.Yes] = { text: uiTr("Yes", "dialog button"), role: DialogButtonBox.YesRole };
    dict[Dialog.YesToAll] = { text: uiTr("Yes to All", "dialog button"), role: DialogButtonBox.YesRole };
    dict[Dialog.No] = { text: uiTr("No", "dialog button"), role: DialogButtonBox.NoRole };
    dict[Dialog.NoToAll] = { text: uiTr("No to All", "dialog button"), role: DialogButtonBox.NoRole };
    dict[Dialog.Abort] = { text: uiTr("Abort", "dialog button"), role: DialogButtonBox.RejectRole };
    dict[Dialog.Retry] = { text: uiTr("Retry", "dialog button"), role: DialogButtonBox.AcceptRole };
    dict[Dialog.Ignore] = { text: uiTr("Ignore", "dialog button"), role: DialogButtonBox.AcceptRole };
    for (var i in dict) if (dict.hasOwnProperty(i)) dict[i].code = i;
    return dict;
  }
  readonly property var actualButtons: {
    if (!Array.isArray(buttons)) return [];
    var list = [];
    for (var i = 0; i < buttons.length; i++) {
      var button = buttons[i];
      if (typeof button === 'number') {
        button = defaultButtons[button];
        if (button) {
          list.push(Object.assign({ index: i }, button));
        }
      } else if (typeof button === 'object') {
        if (button.code !== undefined) {
          button = Object.assign({ index: i }, defaultButtons[button.code], button);
        } else {
          button = Object.assign({ index: i }, button);
        }
        if (button.role === undefined) {
          button.role = typeof button.clicked === 'function' ? DialogButtonBox.ActionRole : DialogButtonBox.AcceptRole;
        }
        list.push(button);
      }
    }
    closePolicy = list.some(function(i) { return i.role === DialogButtonBox.RejectRole; }) ? Popup.CloseOnEscape : Popup.NoAutoClose;
    return list;
  }
  readonly property var buttonsDisabled: {
    var dict = {};
    dict[DialogButtonBox.AcceptRole] = !canAccept;
    dict[DialogButtonBox.RejectRole] = !canReject;
    dict[DialogButtonBox.DestructiveRole] = !canDiscard;
    dict[DialogButtonBox.YesRole] = !canYes;
    dict[DialogButtonBox.NoRole] = !canNo;
    dict[DialogButtonBox.ResetRole] = !canReset;
    dict[DialogButtonBox.ApplyRole] = !canApply;
    return dict;
  }

  id: dialog

  modal: true
  parent: Overlay.overlay
  Overlay.modal: Rectangle {
    id: overlayBackground
    color: "#80000000"
    opacity: dialog.opacity
    Behavior on opacity {
      SmoothedAnimation { velocity: 4.0 }
    }
  }

  // Center using the window's actual logical width/height (see
  // SecondaryWindow); the Overlay's size is incorrect because it always sizes
  // itself to the window size even when transformations are applied to it.
  x: Math.round((parent.Window.window.actualLogicalWidth - width) / 2)
  y: Math.round((parent.Window.window.actualLogicalHeight - height) / 2)
  focus: true

  closePolicy: actualButtons.some(function(i) { return i.role === DialogButtonBox.RejectRole; }) ? Popup.CloseOnEscape : Popup.NoAutoClose;
  opacity: 0.0
  padding: 20
  topPadding: 15
  bottomPadding: 15
  header: Rectangle {
    color: Theme.popup.dialogFrameColor
    implicitHeight: 40
    Text {
      anchors.fill: parent
      anchors.leftMargin: 20
      anchors.rightMargin: 20
      text: dialog.title
      font: dialog.font
      color: Theme.popup.dialogTitleTextColor
      horizontalAlignment: Text.AlignLeft
      verticalAlignment: Text.AlignVCenter
    }
  }
  background: Rectangle {
    color: Theme.popup.dialogBackgroundColor
    border.width: 1
    border.color: Theme.popup.dialogFrameColor
  }
  footer: DialogButtonBox {
    width: parent.width
    alignment: Qt.AlignRight
    background: null
    padding: 20
    topPadding: 15
    spacing: 5
    Repeater {
      model: dialog.actualButtons
      delegate: Button {
        id: dialogButton
        text: modelData.text
        property int standardButton: modelData.code !== undefined ? modelData.code : DialogButtonBox.NoButton
        DialogButtonBox.buttonRole: modelData.role !== undefined ? modelData.role : DialogButtonBox.AcceptRole
        enabled: !buttonsDisabled[DialogButtonBox.buttonRole]
        leftPadding: 20
        rightPadding: 20
        background: Rectangle {
          color: {
            if (!enabled)
              Theme.settings.inputButtonDisabledBackgroundColor
            else if (parent.down)
              Theme.settings.inputButtonPressedBackgroundColor
            else
              Theme.settings.inputButtonBackgroundColor
          }
          radius: 3
          border.width: 2
          border.color: {
            if (!enabled)
              Theme.settings.inputButtonDisabledBorderColor
            else if (parent.activeFocus)
              Theme.settings.inputButtonFocusBorderColor
            else
              color
          }
        }
        contentItem: Text {
          text: parent.text
          font: parent.font
          color: parent.enabled ? Theme.settings.inputButtonTextColor : Theme.settings.inputButtonDisabledTextColor
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignVCenter
        }

        OutlineFocusCue {
          id: focusCue
          anchors.fill: parent
          control: dialogButton
        }
        // For some reason Enter doesn't count as activating a button...
        Keys.onPressed: {
          if(KeyUtil.handlePartialButtonKeyEvent(event, focusCue))
            clicked()
        }

        NativeAcc.Button.name: text
        NativeAcc.Button.onActivated: clicked()

        onClicked: {
          if (modelData.clicked) {
            modelData.clicked(modelData);
          }
          dialog.clicked(modelData);
        }
      }
    }
  }
  enter: Transition {
    SmoothedAnimation { property: "opacity"; velocity: 4.0; from: 0.0; to: 1.0 }
  }
  exit: Transition {
    SmoothedAnimation { property: "opacity"; velocity: 4.0; from: 1.0; to: 0.0 }
  }

  Component.onCompleted: {
    // Annotate the PopupItem as the dialog, since it contains the title,
    // content, buttons, etc.
    // Annotating contentItem isn't as good, it just contains the middle
    // content, not the header/footer.
    dialog.background.parent.NativeAcc.Dialog.name = Qt.binding(function(){return dialog.title})
  }
}
