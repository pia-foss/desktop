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

  ratingEnabled: currentState === uiState.pending
  message: {
    switch (currentState) {
    case uiState.pending:
      return uiTr("Rate your experience with PIA.")
    case uiState.rating_positive:
      if(Daemon.data.flags.includes("trustpilot_feedback")) {
        //: Shown after the user submits a positive rating to ask if they would
        //: rate us on Trustpilot.  The text between brackets ([[....]]) is a
        //: link, please preserve the brackets around the corresponding text.
        //: Do not translate 'Trustpilot' since it is a brand (even though link
        //: text is normally translated in PIA.)
        return uiTr("Thank you for your feedback! We would appreciate it if you rate us on [[Trustpilot]].")
      }
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

  embedLinkClicked: function() { Qt.openUrlExternally("https://www.trustpilot.com/evaluate/privateinternetaccess.com") }

  displayIcon: false
  dismissible: true
  active: {
    if(Daemon.settings.ratingEnabled)
      return Daemon.settings.sessionCount >= 10 && Daemon.data.flags.includes("ratings_1")

    // The ratingEnabled setting is disabled when we dismiss the rating or if we
    // submit one.  If we do submit a rating, we want to keep the notification
    // active temporarily to show the post-rating UI.
    return currentState === uiState.rating_positive ||
      currentState === uiState.rating_negative
  }
  function dismiss () {
    // If we were in a post-rating state, go back to "pending" to ensure the UI
    // is hidden
    currentState = uiState.pending
    Daemon.applySettings({"ratingEnabled": false});
  }

  onRatingFinished: function (value) {
    // Opt out of future notifications
    Daemon.applySettings({"ratingEnabled": false});

    if(value <= 3) {
      currentState = uiState.rating_negative;
    } else {
      currentState = uiState.rating_positive;
    }
  }
}
