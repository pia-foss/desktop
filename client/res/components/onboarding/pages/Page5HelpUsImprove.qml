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
import "../../theme"
import "../../common"
import "../../core"
import "../../daemon"
import PIA.NativeHelpers 1.0

Item {
  id: page5

  StaticImage {
    anchors.horizontalCenter: parent.horizontalCenter
    width: sourceSize.width/2
    height: sourceSize.height/2
    y: 72
    //: Screen reader annotation for the graphic on the "help us improve" page,
    //: which is a checklist.
    label: uiTr("Checklist")
    source: Theme.onboarding.helpUsImproveImage
  }

  StaticText {
    id: title
    y: 190
    width: Math.floor(parent.width * 0.8)
    anchors.horizontalCenter: parent.horizontalCenter
    text: uiTr("Help us improve our service")
    font.pixelSize: 24
    color: Theme.onboarding.sectionHeadlineColor
    wrapMode: Text.WordWrap
    horizontalAlignment: Text.AlignHCenter
  }

  StaticText {
    id: description
    y: title.y + title.height + 20
    width: Math.floor(parent.width * 0.8)
    anchors.horizontalCenter: parent.horizontalCenter
    text: {
      return uiTr("To help us ensure our service's connection performance, you can anonymously share your connection stats with us.") +
        "\n" +
        uiTr("These reports do not contain any personally identifiable information.")
    }
    font.pixelSize: 14
    color: Theme.onboarding.sectionParagraphColor
    wrapMode: Text.WordWrap
    horizontalAlignment: Text.AlignHCenter
  }

  QualityEventsInfoDialog {
    id: infoDialog
  }

  TextLink {
    id: findOutMoreLink
    y: description.y + description.height + 14
    anchors.horizontalCenter: parent.horizontalCenter
    text: uiTr("Find out more")
    underlined: true
    onClicked: infoDialog.open()
  }

  PagePrimaryButton {
    id: acceptButton
    focus: true
    // Rarely, the text above may run longer than we expected it to.  This
    // happens sometimes on some Arabic systems if Qt picks an Arabic font with
    // an unusually large line height.  Move the buttons down so they don't
    // collide with the text in that case.
    y: Math.max(findOutMoreLink.y + findOutMoreLink.height + 10, 365)
    anchors.horizontalCenter: parent.horizontalCenter
    text: uiTr("ACCEPT")
    onClicked: {
      Daemon.applySettings({"serviceQualityAcceptanceVersion": NativeHelpers.getClientVersion()})
      closeAndShowDashboard()
    }
  }

  PagePrimaryButton {
    y: acceptButton.y + 72
    anchors.horizontalCenter: parent.horizontalCenter
    text: uiTr("NO THANKS")
    onClicked: {
      // Setting this to "" has no effect the first time the quick tour
      // is run since it defaults to false-ish (empty string) anyway,
      // but if the user revisits the quick tour and clicks this, it makes sense to disable the setting.
      Daemon.applySettings({"serviceQualityAcceptanceVersion": ""})
      closeAndShowDashboard()
    }
  }
}
