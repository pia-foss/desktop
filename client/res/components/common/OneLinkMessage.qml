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

import QtQuick 2.10
import "../core"
import "../theme"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil

// OneLinkMessage is a Text that embeds one hyperlink into the message, which is
// denoted with [[..]] in the translated text.
//
// Be sure to include a translator comment describing the double square brackets
// with the translatable text.
Item {
  id: oneLinkMessage

  property string text
  property color color
  property color linkColor
  property alias textLineHeight: messageText.lineHeight
  property alias textLineHeightMode: messageText.lineHeightMode
  signal linkActivated()

  implicitWidth: messageText.implicitWidth
  implicitHeight: messageText.implicitHeight

  Text {
    id: messageText
    width: parent.width

    linkColor: oneLinkMessage.linkColor
    text: {
      let markup = oneLinkMessage.text.replace("[[", "<a href='#'>")
      markup = markup.replace("]]", "</a>")
      return markup
    }

    wrapMode: Text.WordWrap
    color: oneLinkMessage.color
    font.pixelSize: 13

    // Annotate the text as a static element, without indicating the link
    NativeAcc.Text.name: Util.stripHtml(text)

    onLinkActivated: {
      oneLinkMessage.linkActivated()
    }
  }

  // Text can't actually set the cursor when pointing to a link, use a MouseArea
  // for that.
  MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.NoButton
    cursorShape: messageText.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
  }

  // Create an element to represent the link.  There's no good way to get the
  // bounds of the actual link text, so the item's bounds are the complete
  // element.
  //
  // This doesn't result in ambiguity for keyboard nav because the main text
  // element isn't a tabstop.  For screen readers, the element descriptions
  // indicate what they are.
  Item {
    id: linkTabstop
    anchors.fill: parent

    activeFocusOnTab: true
    NativeAcc.Link.name: {
      // Extract the link text
      let linkStart = oneLinkMessage.text.indexOf("[[")
      let linkEnd = oneLinkMessage.text.indexOf("]]")
      if(linkStart >= 0 && linkEnd >= 0)
        return oneLinkMessage.text.slice(linkStart+2, linkEnd)
      return ""
    }
    NativeAcc.Link.onActivated: oneLinkMessage.linkActivated()

    Keys.onPressed: {
      if(KeyUtil.handleButtonKeyEvent(event)) {
        focusCue.reveal()
        oneLinkMessage.linkActivated()
      }
    }
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: parent
    control: linkTabstop
  }
}
