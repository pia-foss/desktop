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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "./pages"
import "../core"
import '../theme'

Item {
  id: root
  property int currentPage: 0
  property int numPages: 4

  Item {
    id: backdrop
    property int total_width: Theme.onboarding.backdropWidth
    x: 0 - (total_width - root.width)*currentPage/(numPages - 1)

    Behavior on x {
      NumberAnimation{
        easing.type: Easing.InOutQuad
        duration: Theme.animation.normalDuration
      }
    }
    Image {
        id: backdropImage
        source: Theme.onboarding.backdropImage
        width: Theme.onboarding.backdropImageWidth // width of raw image
        height: root.height
    }
    // StarController has been disabled due to PathArc related crashes
    // In a future release we can re-build the rendering using images
    //    StarController {
    //      min_x: 0 - backdrop.x + Theme.onboarding.starStartLeftPadding
    //      max_x: min_x + root.width - Theme.onboarding.starStartRightPadding
    //    }
  }

  PageWrapper {
    pageIndex: 0
    width: parent.width
    Page1Welcome {
    }
  }
  PageWrapper {
    pageIndex: 1
    Page2Theme {}
  }
  PageWrapper {
    pageIndex: 2
    Page3Customize {}
  }
  PageWrapper {
    pageIndex: 3
    Page4Finish {}
  }

  PageFooter {
    visible: opacity > 0
    opacity: currentPage > 0
    Behavior on opacity {
      NumberAnimation {
        duration: Theme.animation.normalDuration
      }
    }

    height: 40
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottomMargin: 48
    anchors.rightMargin: 58
    anchors.leftMargin: 58
  }
}
