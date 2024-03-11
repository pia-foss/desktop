// Copyright (c) 2024 Private Internet Access, Inc.
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
import QtQuick.Window 2.3
import PIA.CircleMouseArea 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import PIA.NativeHelpers 1.0
import "../core"
import "../theme"
import "qrc:/javascript/util.js" as Util

// InfoTip is an icon that displays a tooltip when pointed.  The tooltip is
// positioned in the top-level app window based on the size of the text given
// and the location of the InfoTip.
Item {
  id: infoTip

  property string tipText
  property string accessibleText: tipText
  // Might look into setting this automatically, but for the moment controls
  // near the top of the window can override it manually to put the popup below.
  property bool showBelow: false
  property var icons: {
    'notification': { image: Theme.popup.tipInfoImg, opacity: 0.5 },
    'settings': { image: Theme.settings.tipInfoImg, opacity: 0.5 },
    'warning': { image: Theme.settings.tipWarningImg, opacity: 1.0 },
  }
  property var icon: icons.notification

  property int cursorShape: Qt.ArrowCursor
  property alias containsMouse: mouseArea.containsMouse
  property alias propagateClicks: mouseArea.propagateClicks

  // By default, InfoTips can be focused with tab/backtab, which causes the
  // tip to appear.  They also have annotations to be read with a screen reader.
  // The parent can override this if it isn't desired (the regions list does
  // this).
  property bool accessible: true

  activeFocusOnTab: accessible

  // Static text isn't really right, it doesn't really express how this control
  // works, but none of the other types are better:
  // - 'Note' maps to a generic 'group' role on Mac OS
  // - 'HelpBalloon' and 'ToolTip' map to a generic 'window' role on Mac OS
  // We could consider using a platform-dependent type if other platforms do
  // support those more specific roles, but it'd have to show a significant
  // benefit to be worth the complexity.
  NativeAcc.Text.name: accessible ? accessibleText : ''

  implicitHeight: infoImg.sourceSize.height / 2
  implicitWidth: infoImg.sourceSize.width / 2

  Image {
    id: infoImg
    anchors.fill: parent
    source: (icon.image || icon)
    opacity: showPopup ? 1.0 : (icon.opacity || 0.5)
  }

  // InfoTip still allows higher MouseAreas to handle click events, such as in
  // the regions list, when a tip is shown for regions that don't support port
  // forwarding, but the region can still be clicked to choose it anyway.
  TransparentMouseArea {
    id: mouseArea
    anchors.fill: parent
    cursorShape: infoTip.cursorShape
    acceptedButtons: propagateClicks ? Qt.NoButton : Qt.AllButtons
    hoverEnabled: true
    enabled: infoTip.visible
  }

  OutlineFocusCue {
    anchors.fill: infoTip
    control: infoTip
    // The info tip already has large margins.
    borderMargin: 0
  }

  // Show the popup when either the cursor points to the mouse area, or the
  // control becomes focused by the keyboard.
  // Note that we never gain focus due to clicks, etc., so we know that focus is
  // always due to the keyboard.
  // This does mean that it's possible to display two InfoTips at once (focus
  // one and point to another), that's probably the least surprising thing to do
  // in that situation and it works fine (the last-shown one appears on top)
  readonly property bool showPopup: mouseArea.containsMouse || activeFocus
  onShowPopupChanged: {
    if(showPopup) {
      popup.computePos()
      popup.open()
    }
    else
      popup.close()
  }

  Popup {
    id: popup

    // Get a property of Window.window, or some default if Window.window is not
    // set.  (It's often unset when showing or hiding an InfoTip control, any
    // sane value can be used in that case)
    function getWindowProp(propName, defaultValue) {
      var window = infoTip.Window.window
      if(window)
        return window[propName]
      return defaultValue
    }

    readonly property real winLogWidth: getWindowProp('width', 100) / getWindowProp('contentScale', 1)
    // When shown in DashboardPopup, popupAddMargin adds additional margin to
    // account for the shadow padding
    readonly property real logicalMargin: Theme.popup.tipWindowMargin+getWindowProp('popupAddMargin', 0)

    // Do some last-minute calculations just before the popup is shown.
    // Most calculations are done declaratively (keep in mind the view could
    // scroll while the popup is open), but the text height has an issue that we
    // have to work around.
    function computePos() {
      // Text seems to have issues reporting a contentHeight change when the
      // string changes.  This affects some conditionally-shown InfoTips like
      // the DNS setting warning.
      //
      // Specifically, if you did the following, the InfoTip would pop up as if
      // the text had contentHeight=0:
      // - have DNS set to PIA DNS
      // - restart the client (so the InfoTip for the DNS warning lays out with
      //   contentHeight=0, as the text is empty)
      // - change the DNS to any setting that displays a warning (and confirm
      //   the prompt)
      // - point to the InfoTip for the warning
      //
      // The Text apparently never emitted a change for its contentHeight,
      // though it does emit it for contentWidth, so clearly it is laying out
      // the text.
      //
      // As a workaround, force a change in another property (that the height
      // computation has a dependency on) so the heights will be recomputed.
      // The property value has to actually change for this to work; here it
      // toggles between 1 and 0.
      popupBackground.heightHackDependency = 1 - popupBackground.heightHackDependency
    }

    // x and y are logical coordinates relative to InfoTip.
    // Center over the InfoTip if possible.  (width is in physical
    // coordinates, divide by scale to get the logical width)
    x: {
      var x = Math.round(infoTip.width/2 - width/2)
      return Util.popupXBindingFixup(popup, infoTip.Window.window,
                                     infoTip.Overlay.overlay, x)
    }
    y: {
      var y = infoTip.showBelow ? infoTip.height + 1 : -balloonWrapper.height - 1
      return Util.popupYBindingFixup(popup, infoTip.Window.window,
                                     infoTip.Overlay.overlay, y)
    }
    // width and height are in physical coordinates, since they are interpreted
    // in the overlay layer.
    width: balloonWrapper.implicitWidth
    height: balloonWrapper.implicitHeight

    modal: false
    focus: false
    closePolicy: Popup.NoAutoClose
    padding: 0
    margins: logicalMargin

    enter: Transition {
      NumberAnimation {
        property: "opacity"
        from: 0
        to: 1
        duration: Theme.animation.quickDuration
      }
    }
    exit: Transition {
      NumberAnimation {
        property: "opacity"
        from: 1
        to: 0
        duration: Theme.animation.quickDuration
      }
    }

    background: Item {}

    Item {
      id: balloonWrapper
      implicitWidth: popupBackground.width
      implicitHeight: popupBackground.height + Theme.popup.tipArrowSize

      // Layer this so the opacity is applied to the balloon and arrow as a
      // single layer instead of blending them individually.
      layer.enabled: true
      layer.textureSize: {
        // This size could be computed by mapping this item's bounds to the
        // window's content item, but that would not create dependencies on all
        // the intermediate transformations.
        // Calculating with the content scale makes a lot of assumptions, but it
        // updates correctly if the scale changes.
        var scale = Window.window ? Window.window.contentScale : 1.0
        return Qt.size(width * scale, height * scale)
      }

      Rectangle {
        id: arrow

        x: {
          // Work around buggy Popup coordinate calculations
          var actualPos = Util.calculatePopupActualPos(popup, popup.parent.Window.window,
                                                       popup.parent.Overlay.overlay)
          return Math.round(infoTip.width / 2 - actualPos.x)
        }
        y: infoTip.showBelow ? 0 : popupText.height - Theme.popup.tipArrowSize
        width: Math.floor(Math.sqrt(2) * Theme.popup.tipArrowSize)
        height: width
        color: Theme.popup.tipBackgroundColor
        rotation: 45
        transformOrigin: Item.TopLeft
      }

      Rectangle {
        id: popupBackground

        x: 0
        y: infoTip.showBelow ? Theme.popup.tipArrowSize : 0
        width: popupText.contentWidth + popupText.leftPadding + popupText.rightPadding
        height: popupText.contentHeight + popupText.topPadding + popupText.bottomPadding + 0*heightHackDependency

        // Dependency used to force a recompuation of the height; see
        // computePos() above.
        property int heightHackDependency: 0

        color: Theme.popup.tipBackgroundColor
        radius: Theme.popup.tipBalloonRadius
      }
    }
    // The text is outside the layer, since the layering affects the text
    // antialiasing (even though the layer texture size is increased for the
    // scale factor, the text would be antialiased as if it was not scaled,
    // resulting in large blurry edges).
    // Putting the text outside the layer ensures that it's rendered at the
    // final resolution and doesn't noticeably affect blending.
    Text {
      id: popupText

      y: popupBackground.y

      // Maximum text width allowed for info tips in this window
      readonly property real maxWidth: {
        var max = popup.winLogWidth - 2*popup.margins

        // The window might set an explicit maximum InfoTip width (the settings
        // window does this so the tips aren't overly wide, and so they're
        // consistent in both vertical/horizontal layouts)
        var windowTipMax = Window.window && Window.window.infoTipMaxWidth
        if(windowTipMax && windowTipMax < max)
          max = windowTipMax

        return max - leftPadding - rightPadding
      }

      // Size the text to minimize whitespace at the right side (balance the
      // line lengths)
      readonly property real balancedWidth: {
        // Skip this computation if the InfoTip isn't visible.  It takes roughly
        // 20-50 ms, which could create a significant delay at startup or on a
        // language change if all InfoTips recomputed their balanced widths.
        //
        // By handling the width this way, we compute it when the InfoTip is
        // shown, and we still recompute it if the text or lanugage changes
        // while it's visible.  One downside is that we unnecessarily lose the
        // computed value when the tip is hidden, but that's not a big cost.
        if(!visible)
          return maxWidth

        return NativeHelpers.balanceWrappedText(maxWidth, font.pixelSize,
                                                tipText)
      }

      width: balancedWidth + leftPadding + rightPadding

      leftPadding: Theme.popup.tipTextMargin
      rightPadding: Theme.popup.tipTextMargin
      // The text seems to add some top padding, probably due to the font
      // being used / line spacing / etc.
      topPadding: Theme.popup.tipTextMargin - 3
      // The bottom does pad correctly, but it looks visually larger due to
      // the padding surrounding the bottom of the line, not the baseline.
      bottomPadding: Theme.popup.tipTextMargin - 3
      wrapMode: Text.Wrap
      text: tipText
      font.pixelSize: Theme.popup.tipTextSizePx
      color: Theme.popup.tipTextColor
    }
  }
}
