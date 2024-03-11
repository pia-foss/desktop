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
import QtQuick.Layouts 1.3
import QtQuick.Window 2.3
import QtGraphicalEffects 1.0
import "../common"
import "../core"
import "../daemon"
import "../theme"
import "../settings"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util

Item {
  id: callingCard

  // Minimum top and bottom margins around the image and text.  The large of
  // (text height + text margins) or (img height + img margins) becomes the
  // item's height.
  readonly property int imgTopMargin: 20
  readonly property int imgBottomMargin: 20
  readonly property int textTopMargin: 25
  readonly property int textBottomMargin: 25

  implicitHeight: featureContent.y + featureContent.height

  property string headlineText
  property string bodyText

  signal messageLinkClicked()

  property string sourceImage
  property string imageAnnotation

  readonly property int imgDisplayWidth: 125
  // The images lay out into a 125-px wide column, but some images have
  // "bleeder" effects that extend beyond that bound.  Set these margins to
  // indicate how much of the image bleeds (in the image's source resolution)
  property int imgDisplayLeftBleed: 0
  property int imgDisplayRightBleed: 0
  // Top/bottom "bleeder" effects are possible too.  These affect how the
  // minimum margins are applied to the image and how the image is centered (the
  // "bleed" part is ignored.)
  property int imgDisplayTopBleed: 0
  property int imgDisplayBottomBleed: 0

  readonly property int lineHorzMargin: 18

  Rectangle {
    x: lineHorzMargin
    y: 0
    width: parent.width - 2*lineHorzMargin
    color: "#979797"
    opacity: 0.25
    height: 1
  }

  Item {
    id: featureContent
    width: parent.width
    implicitHeight: {
      let imgh = featureImage.height+imgTopMargin+imgBottomMargin
      let texth = featureLayout.height+textTopMargin+textBottomMargin
      return Math.max(imgh, texth)
    }

    StaticImage {
      id: featureImage

      // The part of the width that maps to the 125-px wide column
      readonly property int imgSourceNonbleedWidth:
        sourceSize.width - imgDisplayLeftBleed - imgDisplayRightBleed
      readonly property real imgScale: {
        if(sourceSize.width <= 0)
          return 1  // Not loaded yet

        let s = imgDisplayWidth / imgSourceNonbleedWidth
        if(imgSourceNonbleedWidth != imgDisplayWidth * 2) {
          console.warn("Applying noninteger scaling to " + sourceImage +
            " - resize asset to " +
            Math.round(sourceSize.width * s * 2) +
            "x" + Math.round(sourceSize.height * s * 2))
        }
        return s
      }

      x: 56 - Math.round(imgDisplayLeftBleed * imgScale)
      // Center only the "non-bleed" part of the image
      readonly property int imgSourceNonbleedHeight:
        sourceSize.height - imgDisplayTopBleed - imgDisplayBottomBleed
      y: -Math.round(imgDisplayTopBleed * imgScale) + parent.height/2 - (imgSourceNonbleedHeight*imgScale)/2

      label: imageAnnotation
      width: sourceSize.width * imgScale
      height: sourceSize.height * imgScale
      source: sourceImage
    }

    Column {
      id: featureLayout
      x: 236
      width: parent.width - 50 - x
      anchors.verticalCenter: parent.verticalCenter

      StaticText {
        id: featureHeadline
        width: parent.width
        wrapMode: Text.WordWrap
        text: headlineText
        color: Theme.dashboard.textColor
        font.pixelSize: 24
      }

      // Vertical spacer
      Item {
        width: 1
        height: 20
      }

      OneLinkMessage {
        id: featureText
        width: parent.width
        color: Theme.dashboard.textColor
        linkColor: color
        text: bodyText
        textLineHeight: 24
        textLineHeightMode: Text.FixedHeight
        onLinkActivated: callingCard.messageLinkClicked()
      }
    }
  }
}
