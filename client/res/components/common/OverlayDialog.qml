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

  // The buttons at the bottom of the dialog. Each button in the array can be
  // described in three ways:
  // 1. A standard button - a button identifier, like Dialog.Ok, Dialog.Cancel,
  //    etc.
  // 2. A custom button - use an object with the following properties:
  //   - text: Translated text for the button
  //   - role: A DialogButtonBox role.  This determines:
  //     - the button's position in the box (DialogButtonBox orders
  //       appropriately for the current platform by role)
  //     - whether the button causes the dialog to take a default action after
  //       emitting clicked() - accept(), reject(), etc.
  //     - whether the button is enabled (determines the corresponding can*
  //       property below)
  //   - clicked: An optional function to call before emitting clicked() and the
  //     standard Dialog signals (accepted(), rejected(), etc.)
  //   - suppressDefault: If true, prevents the default action from being taken-
  //     the dialog is not automatically closed, and default action signals
  //     (accepted()/rejected/applied()/etc.) are not emitted.
  // 3. A customized standard button - use an object with the "code" property
  //    set to the standard button identifier, and define additional custom
  //    properties to override specific aspects of the button individually
  property var buttons

  // Boolean properties to enable/disable categories of buttons
  property bool canAccept: true
  property bool canReject: true
  property bool canDiscard: true
  property bool canYes: true
  property bool canNo: true
  property bool canReset: true
  property bool canApply: true

  // When a button is clicked, OverlayDialog takes the following actions in
  // order:
  // 1. Calls the button's clicked() function (if present)
  // 2. Emits OverlayDialog.clicked(), with the button object as its argument
  // 3. For AcceptRole or RejectRole, closes the dialog (handled by
  //    QQuickDialog::done()/accept()/reject(), skipped if the button has
  //    suppressDefault)
  // 4. Emits a role-specific signal from the dialog (QQuickDialog::accepted()/
  //    rejected()/applied()/etc., skipped if the button has suppressDefault)
  //
  // The button argument is the button object - it has the properties described
  // above for custom buttons, as well as an 'index' property indicating the
  // button's original index.  (Note that standard buttons are replaced by a
  // complete button object here; the 'code' property indicates standard button
  // role if applicable.)
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
  // Setting a DialogButtonBox as the 'footer' implicitly connects the button
  // box's accepted(), rejected() and clicked() signals to handlers in the
  // Dialog, which close the dialog (for accept/reject).
  //
  // In some cases, the "accept" button must take an asynchronous action and
  // only close the dialog if the action is successful (DedicatedIpAddRow).
  // The button must still have the Accept role, so it is sorted correctly by
  // the button box, but we don't want the default action.
  //
  // Wrap the DialogButtonBox in an Item to prevent its signals from being
  // connected automatically, then recreate that connection manually if desired.
  footer: Item {
    width: parent.width
    implicitHeight: dialogButtonBox.height

    DialogButtonBox {
      id: dialogButtonBox
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

          readonly property var buttonModel: modelData

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
        }
      }

      onClicked: {
        var buttonModel = button.buttonModel

        // The 'clicked' property seems to be lost on buttonModel, probably the
        // Repeater's data model can't handle a function-valued property (?)
        var origButtonModel = actualButtons[button.buttonModel.index]

        // Invoke the button's handler if specified
        if(origButtonModel.clicked) {
          origButtonModel.clicked(origButtonModel)
        }

        // Emit the general clicked() signal
        dialog.clicked(origButtonModel)

        // If default actions aren't wanted, quit here
        if(origButtonModel.suppressDefault)
          return

        switch(origButtonModel.role)
        {
          // Call accept()/reject() like the normal connection to
          // DialogButtonBox::accepted(), this closes the dialog and emits a
          // signal
          case DialogButtonBox.AcceptRole:
            dialog.accept()
            break
          case DialogButtonBox.RejectRole:
            dialog.reject()
            break
          // Emit signals like Dialog::handleClick()
          case DialogButtonBox.ApplyRole:
            dialog.applied()
            break
          case DialogButtonBox.ResetRole:
            dialog.reset()
            break
          case DialogButtonBox.DestructiveRole:
            dialog.discarded()
            break
          case DialogButtonBox.HelpRole:
            dialog.helpRequested()
            break
          // There's no default action signal for YesRole/NoRole (should be
          // handled by a callback), but they still close the dialog by default,
          // which is important for the "Reset All Settings" alert.
          case DialogButtonBox.YesRole:
          case DialogButtonBox.NoRole:
            dialog.close()
            break
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
