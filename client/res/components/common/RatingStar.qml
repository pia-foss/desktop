// Copyright (c) 2023 Private Internet Access, Inc.
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
import PIA.NativeAcc 1.0 as NativeAcc
import "../theme"
import "../common"
import "../core"

Item {
  readonly property int starBaseWidth: 22
  readonly property double starHoverScale: 1.1
  property int maxValue: 5
  property int value

  width: starBaseWidth * starHoverScale
  height: width

  property bool filled: {
    if(hoverIndex >= value) {
      return true;
    }

    return false;
  }

  ButtonArea {
    hoverEnabled: true
    //: Screen reader title for "rating star" button, value ranges from 1 to 5
    name: uiTr("Star rating of: %1").arg(value)
    //: Screen reader description for the action taken by a "rating star"
    //: button.  Value ranges from 1 to 5
    description: uiTr("Submit a star rating of %1 out of 5").arg(value)
    id: mouseArea
    anchors.fill: parent

    onClicked: function () {
      triggerStarClick(value)
    }

    // When the mouse leaves the boundaries of the mouse area, wait a brief period before
    // clearing the hover index.
    //
    // If we don't do this, the hover index is cleared when the mouse moves between each RatingStar
    // and it causes a jarring "flickering" effect.
    Timer {
      id: hoverClearTimer
      interval: 300
      onTriggered: {
        if(hoverIndex === value && !parent.containsMouse) {
          hoverIndex = -1;
        }
      }
    }

    onContainsMouseChanged: function() {
      if(containsMouse) {
        hoverIndex = value
      } else {
        hoverClearTimer.start();
      }
    }
  }


  property int imageWidth: filled ? starBaseWidth * starHoverScale : starBaseWidth
  property double filledImageOpacity: filled ? 1 : 0

  Behavior on imageWidth {
    NumberAnimation { duration: Theme.animation.quickDuration * 0.5 }
    enabled: true
  }
  Behavior on filledImageOpacity {
    NumberAnimation { duration: Theme.animation.quickDuration * 0.5 }
    enabled: true
  }

  // un-filled image
  Image {
    width: imageWidth
    height: imageWidth
    opacity: 1 - filledImageOpacity
    source: Theme.dashboard.ratingStarEmpty
  }

  Image {
    width: imageWidth
    height: imageWidth
    opacity: filledImageOpacity
    source: Theme.dashboard.ratingStarFilled
  }

}
