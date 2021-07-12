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
import PIA.NativeAcc 1.0 as NativeAcc
import "../theme"
import "../daemon"

Item {
  property int hoverIndex: -1
  property int filledIndex: -1
  readonly property var uiState: {
    'pending': 0,
    'loading': 1,
    'error_unknown': 2
  }

  property int currentState: uiState.pending

  function triggerStarClick (value) {
    if (currentState === uiState.error_unknown)
      return;
    currentState = uiState.loading;
    Daemon.submitRating(value, function(error) {
      if (error) {
        switch(error.code) {
        default:
          currentState = uiState.error_unknown;
          break
        }
        resetRatingTimer.start();
      } else {
        // Go back to the pending state, since the rating control will be hidden
        // once the rating logic handles onRatingFinished().  This really only
        // matters though if the rating request ends up being shown again due
        // to a reset for testing.
        currentState = uiState.pending;
        onRatingFinished(value)
      }
    });
  }

  // Upon error, show an error message and show the rating stars again after
  // some time
  Timer {
    id: resetRatingTimer
    interval: 5000
    onTriggered: {
      currentState = uiState.pending;
    }
  }


  RowLayout {
    y: 5
    spacing: 2
    opacity: currentState === uiState.pending ? 1 : 0

    Behavior on opacity {
      NumberAnimation {
        duration: Theme.animation.quickDuration
      }
    }

    RatingStar {
      value: 1
    }
    RatingStar {
      value: 2
    }
    RatingStar {
      value: 3
    }
    RatingStar {
      value: 4
    }
    RatingStar {
      value: 5
    }
  }


  Item {
    id: loadingIndicator
    y: 5
    visible: currentState === uiState.loading

    RowLayout {
      Image {
        id: spinnerImage
        Layout.preferredHeight: 15
        Layout.preferredWidth: 15
        source: Theme.login.buttonSpinnerImage
        RotationAnimator {
          target: spinnerImage
          running: loadingIndicator.visible
          from: 0;
          to: 360;
          duration: 1000
          loops: Animation.Infinite
        }
      }

      Text {
        Layout.leftMargin: 5
        color: Theme.dashboard.textColor
        text: uiTr("Loading")
        font.pixelSize: Theme.dashboard.notificationTextPx
      }
    }
  }

  Item {
    y: 5
    visible: currentState === uiState.error_unknown

      Text {
        Layout.leftMargin: 5
        color: Theme.dashboard.textColor
        text: uiTr("Something went wrong.")
        font.pixelSize: Theme.dashboard.notificationTextPx
      }
    }
  }
