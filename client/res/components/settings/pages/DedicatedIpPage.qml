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
import "."
import "../inputs"
import "../stores"
import "../../client"
import "../../daemon"
import "../../settings"
import "../../common"
import "../../core"
import "../../common/regions"
import "../../theme"

Page {
  ColumnLayout {
    anchors.fill: parent
    spacing: 2

    Item {
      Layout.fillWidth: true
      implicitHeight: dipDescription.y + dipDescription.implicitHeight

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
        links: dedicatedIpList.showAddRow ? [{
          text: uiTr("Get Your Dedicated IP"),
          clicked: function(){Qt.openUrlExternally("https://www.privateinternetaccess.com/pages/client-control-panel/dedicated-ip")}
        }] : []
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

    DedicatedIpList {
      id: dedicatedIpList
      Layout.fillWidth: true
      Layout.fillHeight: true
    }
  }
}
