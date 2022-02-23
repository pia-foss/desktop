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
import "."
import "../../inputs"
import "../../stores"
import "../../../client"
import "../../../daemon"
import "../../../settings"
import "../../../common"
import "../../../core"
import "../../../common/regions"
import "../../../theme"

import "../"
import PIA.BrandHelper 1.0


Page {
  id: dedicatedIpPage

  readonly property bool dedicatedIpExists: {
    return Array.isArray(Daemon.state.dedicatedIpLocations) && Daemon.state.dedicatedIpLocations.length > 0;
  }

  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    Item {
      Layout.fillWidth: true
      Layout.columnSpan: 2
      Layout.preferredHeight: dipDescription.y + dipDescription.implicitHeight

      Image {
        id: dipIcon
        source: Theme.settings.dedicatedIpPageImage
        width: sourceSize.width/sourceSize.height*height
        height: 24
      }

      InputLabel {
        id: dipLabel
        anchors.left: dipIcon.right
        anchors.leftMargin: 10
        text: uiTr("Dedicated IP")
      }

      MessageWithLinks {
        id: dipDescription
        anchors.left: dipLabel.left
        anchors.top: dipLabel.bottom
        anchors.topMargin: 8
        anchors.right: parent.right
        message: uiTr("Secure your remote connections to any asset with a dedicated IP from a country of your choice.  During your subscription, this IP will be yours and yours alone, protecting your data transfers with the strongest encryption out there.")
        links: []
        wrapMode: Text.WordWrap
        color: Theme.settings.inputDescriptionColor

        linkColor: Theme.settings.inputDescLinkColor
        linkHoverColor: Theme.settings.inputDescLinkHoverColor
        linkFocusColor: Theme.settings.backgroundColor
        linkFocusBgColor: Theme.popup.focusCueColor
      }
    }

    Item {
      width: 1
      height: 5
    }

    SettingsButton {
      text: uiTr("Get Your Dedicated IP")
      visible: !dedicatedIpExists
      link: BrandHelper.getBrandParam("getDedicatedIpLink")
    }

    Item {
      width: 1
      visible: !dedicatedIpExists
      height: 15
    }


    DedicatedIpList {
      id: dedicatedIpList
      visible: dedicatedIpExists
      Layout.columnSpan: 2
      Layout.fillWidth: true
      Layout.fillHeight: true
    }

    Rectangle {
      visible: !dedicatedIpExists
      color: Theme.settings.inlayRegionColor
      Layout.columnSpan: 2
      Layout.fillWidth: true
      Layout.preferredHeight: 134 + dedicatedIpAdd.errorHeight + dedicatedIpAdd.messageHeight
      radius: 5

      DedicatedIpAdd {
        id: dedicatedIpAdd
        anchors.fill: parent
        anchors.margins: 20

        Connections {
          target: dedicatedIpPage
          function onDedicatedIpExistsChanged () {
            if(dedicatedIpExists)
              dedicatedIpAdd.clearText();
          }
        }
      }
    }

    Item {
      Layout.columnSpan: 2
      Layout.fillHeight: true
    }
  }
}
