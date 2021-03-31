// Copyright (c) 2021 Private Internet Access, Inc.
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
import PIA.FocusCue 1.0
import "../client"
import "../core"
import "../theme"
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil
import PIA.NativeAcc 1.0 as NativeAcc

// MessageWithLinks is a Text that appends any number of hyperlinks at the end.
// The hyperlinks are appended after the message text, and clicking them calls a
// JS function associated with the link.
//
// MessageWithLinks does not specify the font size, wrap mode, etc., which may
// depend on where this is used (the parent should specify them).
//
// The message and links' text can use Qt's HTML4 subset; they will be wrapped
// in some HTML markup for display.
FocusScope {
  id: messageWithLinks

  // Because of the dual Text objects below used to avoid binding loops, any
  // properties of the Text that the parent wants to set must be set identically
  // for both Text objects.
  //
  // We alias some properties of the visible render text object here, then bind
  // the invisible layout text object's corresponding properties to the render
  // text object.
  //
  // More properties can be added here, they just have to be bound to both text
  // objects correctly.
  property alias font: layoutText.font
  property alias wrapMode: layoutText.wrapMode
  property alias color: layoutText.color
  property alias lineHeight: layoutText.lineHeight
  property alias lineHeightMode: layoutText.lineHeightMode

  // The message text displayed before the links
  property string message

  // The links are defined as an array of objects with the following properties:
  // - text: The text displayed by the link
  // - clicked: A function called (with no parameters) when the link is clicked
  property var links

  // A single link can also be embedded in the message, surrounded by '[[..]]',
  // like OneLinkMessage.  When clicked, the embeddedLinkClicked function is
  // called.
  // This has no effect if the message does not contain '[[...]]'; the embedded
  // link tabstop is created by checking for '[[...]]'.
  property var embedLinkClicked

  // Separator between the multiple links - defaults to a pipe character
  property string linkSeparator: ' &nbsp;|&nbsp; '
  // "Tail" text added to link - space and single right angle quote (per style
  // guide) like " >"
  property string linkTail: links && links.length > 1 ? '' : '&nbsp;&rsaquo;'

  // Color for links when idle or hovered
  property color linkColor
  property color linkHoverColor
  // Colors for links when showing keyboard focus cues
  property color linkFocusColor
  property color linkFocusBgColor

  // Cursor shape when not pointing to any link
  property int cursorShape: Qt.ArrowCursor

  // Whether the 'message' part of the MessageWithLinks is accessible.
  // Normally, it is.  It's turned off for a Notification that is clickable,
  // because in that case the clickable button becomes the main accessibility
  // element.
  property bool messageAccessible: true

  readonly property string linkColorStr: Util.colorToStr(linkColor)
  readonly property string linkHoverColorStr: Util.colorToStr(linkHoverColor)
  readonly property string linkFocusColorStr: Util.colorToStr(linkFocusColor)
  readonly property string linkFocusBgColorStr: Util.colorToStr(linkFocusBgColor)

  // Indices used in buildHtml to indicate "no link" or "the embedded link"
  readonly property int noLinkIdx: -2
  readonly property int embedLinkIdx: -1

  // Link displaying keyboard focus cues, if any (noLinkIdx otherwise).  See
  // linkTabstopRepeater below
  readonly property int focusCueLinkIdx: {
    if(embedLinkTabstop.showFocusCue)
      return embedLinkIdx

    // itemAt() doesn't introduce a dependency on the Repeater's children
    var childrenDependency = linkTabstopRepeater.children
    for(var i=0; i<linkTabstopRepeater.count; ++i) {
      if(linkTabstopRepeater.itemAt(i).showFocusCue)
        return i
    }
    return noLinkIdx
  }

  implicitHeight: layoutText.implicitHeight
  implicitWidth: layoutText.implicitWidth

  function getLinkAnchor(index, pointedLinkIdx, focusLinkIdx) {
    let text = '<a class="'
    if(pointedLinkIdx === index)
      text += 'hover '
    if(focusLinkIdx === index)
      text += 'focus '
    // The link target is just the stringified index
    text += '" href="' + index.toString(10) + '">'
    return text
  }

  // Build the HTML markup.  'pointedLinkIdx' is the index of the pointed link,
  // or noLinkIdx if no link is pointed.  Similarly, 'focusLinkIdx' is the index of the
  // link displaying keyboard focus, or noLinkIdx for none.
  function buildHtml(pointedLinkIdx, focusLinkIdx) {
    // Qt's CSS subset does not support the hover pseudo-class; we have to do it
    // manually by applying a class to the hovered link.
    var text = '<html><style>\n' +
      'a { color: ' + linkColorStr + '; text-decoration: none; }\n' +
      'a.hover { color: ' + linkHoverColorStr + '; }\n' +
      'a.focus { color: ' + linkFocusColorStr + '; background-color: ' + linkFocusBgColorStr + '; }\n' +
      'div { margin-top: 2px; color: #809099 }\n' +
      '</style>\n'
    let messageMarkup = message.replace('[[',
      getLinkAnchor(embedLinkIdx, pointedLinkIdx, focusLinkIdx))
    messageMarkup = messageMarkup.replace(']]', '</a>')
    text += messageMarkup

    // We have to explicitly specify the alignment of the divs, or they'll just
    // be left-aligned by default.
    var divAlign = Client.state.activeLanguage.rtlMirror ? "right" : "left"
    if (links.length) {
      text += '<div align=' + divAlign + '>';
      for(var i=0; i<links.length; ++i) {
        // We don't want links to break across lines - use non-breaking spaces in
        // links.
        var linkText = links[i].text.replace(/ /g, '&nbsp;')

        if (i > 0)
          text += linkSeparator
        text += getLinkAnchor(i, pointedLinkIdx, focusLinkIdx)
        text += linkText + linkTail + '</a>'
      }
      text += '</div>';
    }
    text += '</html>'

    return text
  }

  // Rendering this correctly while avoiding binding loops is somewhat tricky.
  // This is because the layout of the text affects the pointed link (since
  // changing the text could move the link around), but the pointed link affects
  // the HTML markup for the 'text' property.
  //
  // There's no way to restyle the links other than changing the markup, and the
  // Text object isn't smart enough to figure out that only non-layout-affecting
  // data was changed (like the color) - changing the text causes a full layout
  // (and thus binding loops).
  //
  // This actually did work most of the time though, because the second layout
  // evaluation would almost always terminate the loop - QML would emit warnings
  // but the resulting layout was correct.  However, there are corner cases in
  // which it does not work, such as with the 'update' notification - if it
  // became visible while the dashboard was shown, it wouldn't lay out
  // correctly.
  //
  // To solve this, the message is laid out and rendered using two separate Text
  // objects:
  // - An invisible 'layout' Text object is used to size the MessageWithLinks
  //   and detect the pointed link.  This Text *cannot* depend on anything
  //   computed from the pointed link.  (Its HTML is always built as if no links
  //   are highlighted.)
  // - A visible 'render' Text object is used to actually display the text.  Its
  //   text can depend on the pointed link, but we cannot depend on any layout
  //   metrics from this text.
  //
  // On the plus side, if (somehow) the link CSS did cause the layout to change,
  // this avoids infinite loops; the links' 'hit regions' always act as if the
  // links are not highlighted.  (The pointed link text might not actually align
  // with the hit regions though in that case.)

  // This is the visible 'render' text object
  Text {
    id: renderText
    anchors.fill: parent

    NativeAcc.Text.name: messageWithLinks.messageAccessible ? messageWithLinks.message : ''

    textFormat: Text.RichText
    text: {
      // The hovered link is determined by layoutText
      var pointedLinkIdx = layoutText.hoveredLink ? parseInt(layoutText.hoveredLink) : noLinkIdx
      return buildHtml(pointedLinkIdx, focusCueLinkIdx)
    }
    // Bind in all properties that the parent could control from layoutText
    font: layoutText.font
    wrapMode: layoutText.wrapMode
    color: layoutText.color
    lineHeight: layoutText.lineHeight
    lineHeightMode: layoutText.lineHeightMode
  }

  // This is the inivisible 'layout' text object
  Text {
    id: layoutText
    // Set the width of the layout text to the parent, but not the height
    x: 0
    y: 0
    width: parent.width
    // Not visible - this text doesn't highlight links.  This done by setting
    // opacity to 0 instead of visible to false so the text object still handles
    // cursor interaction.
    opacity: 0
    textFormat: Text.RichText
    text: {
      // This is key - we never pass a pointed link index when building the HTML
      // for the layout text.  As a result, it doesn't depend on the highlighted
      // link, which eliminates the binding loops.
      return buildHtml(noLinkIdx, noLinkIdx)
    }

    onLinkActivated: {
      var linkIdx = parseInt(link)
      if(linkIdx == embedLinkIdx) {
        embedLinkTabstop.forceActiveFocus(Qt.MouseFocusReason)
        embedLinkClicked()
      }
      else if(linkIdx >= 0) {
        // Focus the tab stop for this link
        var linkTabstop = linkTabstopRepeater.itemAt(linkIdx)
        if(linkTabstop)
          linkTabstop.forceActiveFocus(Qt.MouseFocusReason)
        links[linkIdx].clicked()
      }
    }
  }

  // Text can't actually set the hand cursor when pointing to a link.
  MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.NoButton
    cursorShape: layoutText.hoveredLink ? Qt.PointingHandCursor : messageWithLinks.cursorShape
  }

  // Keyboard navigation is handled by creating Items for each link that are tab
  // stops.  If an Item gains focus and displays focus cues, focusCueLinkIdx
  // is set to the appropriate index.  That in turn causes the CSS to apply a
  // highlight to the link.
  //
  // The bounds of these items _do_ matter (slightly), they're used on the
  // Connect page to automatically scroll to the item when it's focused with the
  // keyboard.  They fill the entire message as a relatively-sane default.
  // This isn't perfect if the item has multiple links, but we don't have a
  // great way to get the links' actual positions.

  // This item is for the embedded link, if there is one
  Item {
    id: embedLinkTabstop
    anchors.fill: parent
    activeFocusOnTab: true

    readonly property string embedLinkText: {
      let startBracketPos = messageWithLinks.message.indexOf('[[')
      let endBracketPos = messageWithLinks.message.indexOf(']]')
      if(startBracketPos == -1 || endBracketPos == -1)
        return ""

      return messageWithLinks.message.slice(startBracketPos+2, endBracketPos)
    }

    visible: !!embedLinkText
    NativeAcc.Link.name: embedLinkText
    NativeAcc.Link.onActivated: messageWithLinks.embedLinkClicked()

    readonly property bool showFocusCue: embedLinkFocusCue.show
    OutlineFocusCue {
      id: embedLinkFocusCue
      anchors.fill: parent
      control: embedLinkTabstop
    }

    Keys.onPressed: {
      if(KeyUtil.handleButtonKeyEvent(event)) {
        embedLinkFocusCue.reveal()
        messageWithLinks.embedLinkClicked()
      }
    }
  }
  Repeater {
    id: linkTabstopRepeater
    // QML seems to not propagate the 'clicked' property of the links when they
    // are used as a data model, probably since it's a function.  Just create
    // the appropriate number of tab stops and find the link by index when it's
    // clicked.
    model: links.length
    Item {
      id: linkTabstop
      anchors.fill: parent
      activeFocusOnTab: true

      NativeAcc.Link.name: links[index] ? links[index].text : ''
      NativeAcc.Link.onActivated: links[index] && links[index].clicked()

      readonly property bool showFocusCue: focusCue.show

      // We do show an outline if any link is focused, because the links'
      // individual focus cues aren't very clear that they represent keyboard
      // focus (Qt's limited CSS support limits these).
      // This isn't ideal for messages with more than one link, but we don't
      // actually have any of these right now.  If we keep the links on their
      // own lines, we might refactor this to manually make the links out of
      // Text items and show individual focus cues.
      OutlineFocusCue {
        id: focusCue
        anchors.fill: parent
        control: linkTabstop
      }

      Keys.onPressed: {
        if(KeyUtil.handleButtonKeyEvent(event)) {
          focusCue.reveal()
          var link = links[index]
          if(link)
            link.clicked()
        }
      }
    }
  }
}
