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

import QtQuick 2.11
import PIA.NativeAcc 1.0 as NativeAcc
import PIA.Clipboard 1.0
import QtQuick.Layouts 1.3
import "../core"
import "../theme"
import "qrc:/javascript/keyutil.js" as KeyUtil

// CopiableValueText is a ValueText which provides clipboard-copy functionality.

// It enables text to be copied to the clipboard when it is clicked, but only when the copiable property
// is set to true.
// A small 'copy' icon is displayed to the right of the text to indicate the text may be copied.
Item {
  id: copiableValueText

  // Must be true for text to be copiable
  property bool copiable: false

  property alias font: valueText.font
  property alias text: valueText.text
  property alias label: valueText.label
  property alias color: valueText.color

   // When in a key press, the key that was pressed
  property var inKeyPress: null

  implicitWidth: valueText.implicitWidth
  implicitHeight: valueText.implicitHeight

  ValueText {
    id: valueText
    anchors.fill: parent
    NativeAcc.ValueText.copiable: copiable
    rightPadding: image.width + 3
    activeFocusOnTab: true

    Keys.onPressed: {
      if (KeyUtil.handleButtonKeyEvent(event)) {
        focusCue.reveal()
        inKeyPress = event.key
      }
    }

    Keys.onReleased: {
      if (inKeyPress && inKeyPress === event.key) {
        event.accepted = true
        inKeyPress = null
        if (copiable) Clipboard.setText(valueText.text)
      }
    }
  }

  MouseArea {
    id: mouseArea
    anchors.fill: parent
    hoverEnabled: true
    cursorShape: copiable && containsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor

    onClicked: {
      if (copiable) Clipboard.setText(valueText.text)
    }
  }

  Image {
    id: image
    visible: copiable && (mouseArea.containsMouse || focusCue.show)
    source: Theme.dashboard.clipboardImage
    opacity: (mouseArea.containsPress || inKeyPress) ? 1.0 : 0.4

    width: (height / sourceSize.height) * sourceSize.width
    height: valueText.font.pixelSize
    anchors.right: parent.right
    anchors.bottom: valueText.baseline
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: parent
    control: valueText
  }
}
