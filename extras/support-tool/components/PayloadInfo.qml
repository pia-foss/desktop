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

import QtQuick 2.0
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import PIA.ReportHelper 1.0

Item {
  ScrollView {
    anchors.fill: parent
    anchors.margins: 2
    anchors.rightMargin: 10
    contentWidth: width
    contentHeight: infoContents.height
    clip: true

    Flickable {
      anchors.fill: parent
      // Disable bouncy overscroll, which is a bizarre default for desktop
      // platforms
      boundsBehavior: Flickable.StopAtBounds

      Column {
        id: infoContents
        width: parent.width - 20
        spacing: 10
        Text {
          visible: params.mode == "logs"
          text: "<p>" + ReportHelper.getBrandParam("brandName") + " will upload debug logs and some information about your network setup. This information doesn't contain any information about your internet usage.</p>"
          wrapMode: Text.Wrap
          width: parent.width
        }
        Text {
          visible: params.mode == "crash"
          text: "<p>Please re-start the application. If problem persists, please contact support.</p><br/><p>If you have a killswitch enabled your internet might not be accessible. Restarting the application should correct this.</p>"
          wrapMode: Text.Wrap
          width: parent.width
        }
        FileList {
          width: parent.width
        }
      }
    }
  }
}
