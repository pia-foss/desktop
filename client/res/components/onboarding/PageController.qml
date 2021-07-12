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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.11
import "./pages"
import "../core"
import '../theme'
import "../daemon"
import "../client"

Item {
  id: root
  readonly property int currentPage: Client.uiState.onboarding.currentPage
  readonly property bool showHelpImprove: Daemon.data.flags.includes("service_quality_events")
  property int numPages: showHelpImprove ? 5 : 4

  // In the extreme case that showHelpImprove becomes false while we're actually
  // on that page, go back to the prior page.
  onShowHelpImproveChanged: {
    if(!showHelpImprove && Client.uiState.onboarding.currentPage == pageIndices.helpimprove)
      Client.uiState.onboarding.currentPage = pageIndices.finish
  }

  readonly property var pageIndices: ({
    welcome: 0,
    theme: 1,
    customize: 2,
    finish: 3,
    helpimprove: 4
  })

  Item {
    id: backdrop
    property int total_width: Theme.onboarding.backdropWidth
    x: 0 - (total_width - root.width)*currentPage/(numPages - 1)

    Behavior on x {
      NumberAnimation{
        easing.type: Easing.InOutQuad
        duration: Theme.animation.normalDuration
      }
      enabled: root.Window.window && root.Window.window.visible
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
  PageWrapper {
    anchors.fill: parent
    pageIndex: 4
    Page5HelpUsImprove {anchors.fill: parent}
  }

  PageFooter {
    visible: opacity > 0
    opacity: currentPage !== pageIndices.welcome && currentPage !== pageIndices.helpimprove
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

    showHelpImprove: root.showHelpImprove
  }
}
