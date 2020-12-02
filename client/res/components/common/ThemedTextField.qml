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
import QtQuick.Controls 2.4
import QtQuick.Window 2.11
import "../theme"
import "../common"
import "../core"
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util

// This TextField is used for most text fields in the application; it has a
// themed context menu and cursor selection enabled.
//
// It does not currently set any colors or font sizes; it's used in different
// contexts that use different values for those.
TextField {
  property bool masked: false
  // Label - the control's name for screen reader annotations.  Usually either a
  // nearby StaticText's text or the placeholder text.
  property string label
  // Whether this is a search text field (screen reader hint)
  property bool searchEdit: false

  id: control
  echoMode: masked ? TextInput.Password : TextInput.Normal
  selectByMouse: true

  NativeAcc.TextField.name: label
  NativeAcc.TextField.value: text
  NativeAcc.TextField.cursorPos: cursorPosition
  NativeAcc.TextField.passwordEdit: masked
  NativeAcc.TextField.searchEdit: searchEdit
  NativeAcc.TextField.onShowMenu: showContextMenu(0, height)
  NativeAcc.TextField.onChangeCursorPos: cursorPosition = newPos

  function showContextMenu(x, y) {
    // As with other popups, fix up the X and Y position to work around Popup
    // bugs with Overlay layer transformations.
    var pos = Util.popupXYBindingFixup(contextMenu, contextMenu.parent.Window.window,
                                       contextMenu.parent.Overlay.overlay, x, y)
    // The context menu will take the focus, which normally would cause the
    // text field to clear its selection.  We want to keep it in this case, so
    // set the property to true just while we pop the menu.  (We still want it
    // to clear the selection if it loses focus otherwise.)
    control.persistentSelection = true
    contextMenu.popup(pos)
    control.persistentSelection = false
  }

  // Despite the fact that the doc for the textEdited signal specifically says
  // that it's not invoked due to programmatic changes in the text, it actually
  // is invoked by remove().
  //
  // This handler just deletes line breaks, but we'd still have to be really
  // careful to handle reentrancy correctly - instead, just flag that we're
  // altering the text and ignore reentrant signals.
  property bool handlingEdit: false
  onTextEdited: {
    if(handlingEdit)
      return
    try {
      handlingEdit = true

      // Despite the fact that TextField is always single-line, it will still
      // paste line break characters, which are often copied accidentally (it
      // definitely happens when copying DIP tokens from a code block on a GitLab
      // wiki page, probably happens in many other circumstances).
      //
      // Clean line break characters after pasting.  This works fine for pasting a
      // single line.  Pasting multiple lines might would probably be better off
      // inserting spaces for line breaks that weren't at the end of the text, but
      // Qt doesn't give us a chance to intercept the clipboard text, we have to
      // fix it up later.
      //
      // Since these edits are always single-line, we can safely clean line breaks
      // in the whole control, they're never supposed to be there. Explicitly
      // delete characters rather than replacing the whole string to preserve the
      // cursor position, etc.
      var newText = control.text
      for(var i=newText.length-1; i>=0; --i) {
        var c = newText.charAt(i)
        if(c === '\r' || c === '\n') {
          control.remove(i, i+1)
        }
      }
    }
    finally {
      handlingEdit = false
    }
  }

  // There's no way to get to the context menu from screen readers - the QML
  // Accessible type doesn't allow adding custom actions.
  // The 'menu' key and Ctrl+click (on Mac) also aren't handled right now.
  MouseArea {
    id: contextMenuMouseArea
    anchors.fill: parent
    acceptedButtons: Qt.RightButton
    cursorShape: Qt.IBeamCursor
    onPressed: showContextMenu(mouse.x, mouse.y)

    ThemedMenu {
      id: contextMenu
      menuWidth: 130
      itemHeight: 20
      topPadding: 4
      bottomPadding: 4

      readonly property bool hasSelection: control.selectionStart !== control.selectionEnd
      readonly property real separatorHeight: 10

      function focusControl() {
        control.forceActiveFocus()
      }

      Action {
        text: uiTr("Undo")
        enabled: control.canUndo
        onTriggered: {
          control.undo()
          contextMenu.focusControl()
        }
      }
      Action {
        text: uiTr("Redo")
        enabled: control.canRedo
        onTriggered: {
          control.redo()
          contextMenu.focusControl()
        }
      }
      ThemedMenuSeparator {}
      // Cut and Copy are disabled for masked (password) inputs; this seems to
      // be
      Action {
        text: uiTr("Cut")
        enabled: contextMenu.hasSelection && !masked
        onTriggered: {
          control.cut()
          contextMenu.focusControl()
        }
      }
      Action {
        text: uiTr("Copy")
        enabled: contextMenu.hasSelection && !masked
        onTriggered: {
          control.copy()
          contextMenu.focusControl()
        }
      }
      Action {
        text: uiTr("Paste")
        enabled: control.canPaste
        onTriggered: {
          control.paste()
          contextMenu.focusControl()
        }
      }
      Action {
        text: uiTr("Delete")
        enabled: contextMenu.hasSelection
        onTriggered: {
          control.remove(control.selectionStart, control.selectionEnd)
          contextMenu.focusControl()
        }
      }
      ThemedMenuSeparator {}
      Action {
        text: uiTr("Select All")
        enabled: true
        onTriggered: {
          contextMenu.focusControl()
          control.selectAll()
        }
      }

      onVisibleChanged: {
        // Since we allowed the selection to persist when showing the context
        // menu, clear it if the menu is dismissed, but the control has not
        // been focused.  (This happens if the user clicks away from the menu
        // without taking any action.)
        //
        // Clear the selection, but keep the caret position, like the TextField
        // normally does (by setting the selection range to the old caret position)
        if(!visible && !control.activeFocus)
           control.select(control.cursorPosition, control.cursorPosition)
      }
    }
  }

  OutlineFocusCue {
    anchors.fill: parent
    control: parent
  }
}
