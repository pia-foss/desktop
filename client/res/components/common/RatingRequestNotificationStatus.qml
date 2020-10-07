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
import PIA.NativeHelpers 1.0
import PIA.BrandHelper 1.0

import "../daemon"

NotificationStatus {
  readonly property var uiState: {
    "pending": 0,
    "rating_positive": 1,
    "rating_negative": 2
  }

  property int currentState: uiState.pending

  // Keep a copy of "ratingEnabled" as is when the app is started up.
  // visibility depends on session count, and ratings enabled.
  //
  // If we use Daemon.settings.ratingsEnabled directly to determine visibility,
  // the notification will be hidden upon successful rating submission (at which point we set that flag to false)
  property bool ratingEnabledOnStartup: false

  ratingEnabled: currentState === uiState.pending
  message: {
    switch (currentState) {
    case uiState.pending:
      return uiTr("Rate your experience with PIA.")
    case uiState.rating_positive:
      return uiTr("Thank you for your feedback!")
    case uiState.rating_negative:
      return uiTr("Thank you! If you encounter any problems, please contact support.")
    }
  }

  links: {
    if (currentState === uiState.rating_negative) {
      return [{
                "text": uiTr("Contact Support"),
                "clicked": function () {
                   Qt.openUrlExternally(BrandHelper.getBrandParam("helpDeskLink"))
                }
              }]
    }

    return []
  }

  displayIcon: false
  dismissible: true
  active: ratingEnabledOnStartup && Daemon.settings.sessionCount >= 10 && Daemon.data.flags.includes("ratings_1")
  function dismiss () {
    ratingEnabledOnStartup = false;
    Daemon.applySettings({"ratingEnabled": false});
  }


  onRatingFinished: function (value) {
    // Opt out of future notifications
    // but do not reset ratingEnabledOnStartup because that would hide the notification
    // immediately
    Daemon.applySettings({"ratingEnabled": false});

    if(value <= 3) {
      currentState = uiState.rating_negative;
    } else {
      currentState = uiState.rating_positive;
    }
  }

  Component.onCompleted: {
    // RatingEnabled is set to true by default.
    // Set a small timer to defer reading the daemon setting value, so we get the correct value
    initializeTimer.start();
  }

  property var initializeTimer: Timer {
    interval: 100
    onTriggered: {
      ratingEnabledOnStartup = !!Daemon.settings.ratingEnabled;
    }
  }
}
