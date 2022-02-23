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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../theme"
import "../common"
import "../core"
import "../client"

Item {
  id: pageFooter
  property bool showHelpImprove

  // "Skip Tour" button - hidden for the last page
  Item {
    visible: currentPage < numPages - 1
    Text {
      id: skipTourText
      color: Theme.onboarding.skipTourColor
      text: uiTr("SKIP TOUR")
      anchors.verticalCenter: parent.verticalCenter
    }
    height: 40
    width: 150
    anchors.left: parent.left

    ButtonArea {
      anchors.fill: parent
      name: skipTourText.text
      cursorShape: Qt.PointingHandCursor
      onClicked: {
        closeAndShowDashboard();
      }
    }
  }

  // Navigation dots.  The "0th" page (the welcome page) and the "4th" page
  // ("help us improve") can't be reached via a navigation dot and do not
  // display the footer.
  Item {
    width: 58
    height: 10
    anchors.centerIn: parent

    NavigationDot {
      x: 0
      targetPage: 1
    }
    NavigationDot {
      x: 24
      targetPage: 2
    }
    NavigationDot {
      x: 48
      targetPage: 3
    }
  }

  // The "Next" / "Log In" button
  //
  // The old flow (without the "Help us improve" page, still present if we
  // unpublish the feature flag for service quality events) showed "Next" on
  // every page until the last, which then showed "Log In".
  //
  // The new flow with the "Help us improve" page shows "Next" on ever page but
  // the last, and the last page hides the button entirely (the
  // "Help us improve" page has its own buttons that end the flow).
  //
  // So show "Log In" when on the last page, and when the "Help us improve" page
  // is not being used.  (Even though the button is hidden, don't show "Log In"
  // on the "Help us improve" page because it'd be briefly visible during the
  // fade-out.)
  Item {
    anchors.right: parent.right
    width: 150
    height: 40
    property bool useLogInButton: {
      return currentPage === (numPages - 1) && !pageFooter.showHelpImprove
    }

    Image {
      anchors.fill: parent
      source: Theme.onboarding.nextButtonImage
    }

    Text {
      id: nextButtonText
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.verticalCenter: parent.verticalCenter
      text: parent.useLogInButton ? uiTr("LOG IN") : uiTr("NEXT")
      color: Theme.onboarding.buttonTextColor
    }

    ButtonArea {
      anchors.fill: parent
      name: nextButtonText.text
      cursorShape: Qt.PointingHandCursor
      onClicked: {
        if(parent.useLogInButton) {
          closeAndShowDashboard()
        }
        else if(Client.uiState.onboarding.currentPage < (numPages-1)) {
          Client.uiState.onboarding.currentPage += 1
        }
      }
    }
  }

}
