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

import QtQuick 2.11 as QtQuick
import QtQuick.Window 2.10 as QtQuickWindow
import "../client"

// Text tweaks:
// - Don't mirror the text itself for RTL
// - Don't detect alignment from text content, just left-align by default and
//   reverse the alignment for RTL
QtQuick.Text {
  id: coreText

  // Always left-align by default.  (This can be overridden when right- or
  // center-aligned text is needed.)
  //
  // The default Text behavior would be to detect based on the content - for
  // Arabic, the text would become right-aligned.
  //
  // This is useless (for us at least) for several reasons:
  // - It depends on the Text object being laid out in a specific way - it has to
  //   actually be sized to the area in which it's aligned, which isn't the
  //   default (by default its width is its implicitWidth and horizontal
  //   alignment has no effect).
  // - It doesn't always match the UI layout direction.  We display non-Arabic
  //   text in RTL (which should still be right-aligned), and Arabic text in LTR
  //   (the Arabic selection in the Language drop-down, which should be left-
  //   aligned).
  //
  // Instead, always left-align by default.  UI mirroring (if/when that's
  // supported) will flip this correctly because it's an explicit binding.
  //
  // Regardless of horizontal alignment, reverse the alignment for RTL.
  // We do _not_ do this with LayoutMirroring.enabled, because that also
  // reverses anchors, ugh.
  //
  // Instead, manually reverse the alignment if it's left or right and mirroring
  // is enabled.  The "effectiveHorizontalAlignment" property still works
  // normally.

  // The implementation of the alignment tweaks involves some aliases in order
  // to hide the real 'horizontalAlignment' property.  Bindings should refer to
  // 'horizontalAlignment' normally.
  property int ltrHorizontalAlignment: QtQuick.Text.AlignLeft
  property alias horizontalAlignment: coreText.ltrHorizontalAlignment

  // The QML doc says that we should be able to create a "horizontalAlignment"
  // binding here and bind the real property of Text itself.  However, this
  // does not seem to work; the binding is just silently ignored (it doesn't set
  // _either_ property).  Even the sample code on the linked page does this if
  // you actually try to run it.
  //
  // Using a second alias to reach the real property does work though.
  property alias actualHorizontalAlignment: coreText.horizontalAlignment
  actualHorizontalAlignment: {
    if(Client.state.activeLanguage.rtlMirror && rtlAlignmentMirror) {
      if(ltrHorizontalAlignment === QtQuick.Text.AlignLeft)
        return QtQuick.Text.AlignRight
      if(ltrHorizontalAlignment === QtQuick.Text.AlignRight)
        return QtQuick.Text.AlignLeft
    }

    // When mirroring is off, or if the alignment is any other value, there's no
    // change.
    return ltrHorizontalAlignment
  }

  // In some rare cases (the Changelog), the text alignment shouldn't change
  // even when the client is RTL.  Set rtlAlignmentMirror to false to keep the
  // specified alignment.
  property bool rtlAlignmentMirror: true

  readonly property real rtlFlip: QtQuickWindow.Window.window ? QtQuickWindow.Window.window.rtlFlip : 1
  // Don't flip the text content in RTL.
  // (Apply a mirror transformation to the text, so it ends up back in its
  // original layout when the UI is mirrored).
  transform: [
    QtQuick.Scale {
      // Flip around the center of the actual text, not including the padding,
      // so the padding stays on the correct sides.
      readonly property real textWidth: width - leftPadding - rightPadding
      origin.x: leftPadding + textWidth/2
      xScale: coreText.rtlFlip
    }
  ]
}
