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
import "../core"
import PIA.FocusCue 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util

// This is a basic parser for Markdown text that renders the elements as
// individual Text items.  Rendering this separately is important for
// accessibility; one giant text element is hard (or impossible) to navigate
// with most screen readers.
//
// There's no way to get the location of a range of text from Text itself (even
// though it clearly has this information to implement linkAt()), so we have to
// split it into individual elements.
//
// The Markdown parsing is very basic.  It only supports:
// Headings: `# ` (h1) through `###### ` (h6)
// Unordered list items: `* `
// Paragraphs: any other text
//
// Paragraph separation isn't exactly like proper Markdown.  Paragraphs are
// just broken on '\n' and blank lines are ignored.  (Proper Markdown breaks
// paragraphs on blank lines only; line breaks within paragraphs are joined.)
Item {
  id: markdownPage

  property real margins: 0
  property real fontPixelSize: 13
  property string text
  property color color

  implicitHeight: elementColumn.totalImplicitHeight + 2*margins

  // List of elements extracted from the Markdown content.
  // Elements are objects with the following attributes:
  // - type: 'paragraph', 'heading', 'listitem'
  // - level: (heading type only) 1-4
  // - content: Text inside element - for bullets and headings, excludes the
  //   '* ' / '#[#...] '.
  // - padBelow: Padding below this item (in px); used to provide separation
  //   between elements, except between two consecutive list items
  //
  readonly property var elements: {
    var elements = []
    var lines = text.split('\n')
    var line, match
    var priorElement
    var nextElement
    for(var i=0; i<lines.length; ++i) {
      line = lines[i]
      if(line.length === 0)
        continue  // Blank line; ignore it

      // Intentional assignments in conditions
      if(match = line.match(/\*\s(.+)/))
        nextElement = {type: 'listitem', content: match[1], padBelow: 0}
      else if(match = line.match(/^(#+)\s(.+)/))
        nextElement = {type: 'heading', level: match[1].length, content: match[2], padBelow: 0}
      else
        nextElement = {type: 'paragraph', content: line, padBelow: 0}

      // Insert padding between any elements except two consecutive list items
      if(priorElement && (priorElement.type !== 'listitem' || nextElement.type !== 'listitem')) {
        priorElement.padBelow = fontPixelSize

        // Slightly light less padding for heading->list item.  This was
        // measured and might be specific to the changelog font size.
        if(priorElement.type === 'heading' && nextElement.type === 'listitem')
            priorElement.padBelow -= 2
      }
      elements.push(nextElement)
      priorElement = nextElement
    }
    return elements
  }

  Column {
    id: elementColumn
    anchors.fill: parent
    anchors.margins: parent.margins

    readonly property real actualTotalImplicitHeight: {
      var h = 0
      for(var i=0; i<children.length; ++i) {
        h += children[i].implicitHeight
      }
      return h
    }

    // The binding above is recomputed as each text element is generated.  For
    // the changelog, this generates way too many height change events and
    // confuses the scroll view; it stops updating the height at some point.
    //
    // Defer the change to work around the scroll view bugs.
    property real totalImplicitHeight: 1
    Timer {
      id: heightUpdateTimer
      repeat: false
      interval: 0
      onTriggered: elementColumn.totalImplicitHeight = elementColumn.actualTotalImplicitHeight
    }
    onActualTotalImplicitHeightChanged: heightUpdateTimer.start()

    Repeater {
      model: markdownPage.elements

      // Padding wrapper (padding is not included in Text bounds so the screen
      // reader cursor does not include it)
      Item {
        width: parent.width
        implicitHeight: Math.ceil(textElement.implicitHeight + modelData.padBelow)

        Text {
          id: textElement
          width: parent.width
          font.pixelSize: markdownPage.fontPixelSize
          color: markdownPage.color
          wrapMode: Text.WordWrap
          textFormat: Text.RichText

          // Never mirror the alignment.  This is used for the changelog and beta
          // agreement, which are always in English.
          rtlAlignmentMirror: false

          NativeAcc.Text.name: modelData.content

          // These items can be focused by screen readers.  We need to scroll the
          // view when the screen reader navigates through the items, and focus
          // notifications are the only way to know where the screen reader cursor
          // is.  They're not focusable normally in any way; there's no reason to
          // allow this.
          //
          // The only slight side effect is that navigating through the text with
          // a screen reader removes focus from any other control (it does not
          // normally do that for text elements); but navigating to another
          // control will refocus it normally.
          //
          // The user _can_ disable syncing of keyboard focus with the screen
          // reader cursor (on Mac at least), in which case the view won't be able
          // to scroll automatically.
          NativeAcc.Text.alwaysFocusable: true

          text: {
            switch(modelData.type) {
              default:
              case 'paragraph':
                return modelData.content
              case 'heading':
                return "<h" + modelData.level + ">" + modelData.content + "</h" + modelData.level + ">"
              case 'listitem':
                return "<ul><li>" + modelData.content + "</li></ul>"
            }
          }

          // Provide an invisible FocusCue so the FocusCure.childCueRevealed
          // attached signal is emitted, which causes the view to scroll
          FocusCue {
            anchors.fill: parent
            control: parent
          }
        }
      }
    }
  }
}
