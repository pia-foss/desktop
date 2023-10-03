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

import QtQuick 2.0
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import PIA.ReportHelper 1.0

Item {
  ColumnLayout {
    anchors.fill: parent
    spacing: 0
    Rectangle {
      Layout.fillWidth: true
      color: "#2b2e39"
      height: 65
      RowLayout {
        anchors.fill: parent
        anchors.margins: 15
        Text {
          color: "#fff"
          font.pixelSize: 24
          text: rootWindow.title
          Layout.fillWidth: true
        }
        Text {
          text: qsTr("Support Helpdesk")
          color: "#5ddf5a"
          font.underline: true
          MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: Qt.openUrlExternally(ReportHelper.getBrandParam("helpDeskLink"))
          }
        }
      }
    }
    Rectangle {
      height: 2
      Layout.fillWidth: true
      color: "#2b2e39"
    }

    Rectangle {
      Layout.fillHeight: true
      Layout.fillWidth: true
      RowLayout {
        anchors.fill: parent
        anchors.margins: 15
        GroupBox {
          Layout.fillHeight: true
          Layout.preferredWidth: 300
          title: "Information"
          PayloadInfo {
            width: 300
            height: parent.height
          }
        }
        GroupBox {
          Layout.fillHeight: true
          Layout.fillWidth: true
          title: {
            if(params.mode == "crash") return qsTr("Report Error")
            if(params.mode == "logs") return qsTr("Send Debug Logs")
            return "Send Report"
          }
          SubmitForm {
            anchors.fill: parent
          }
        }
      }
    }

    Rectangle {
      height: 1
      color: "#bebebe"
      Layout.fillWidth: true
    }
    Rectangle {
      color: "#889099"
      Layout.fillWidth: true

      height: 60
      RowLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10
        Item {
          // spacer
          Layout.fillWidth: true
        }

        Button {
          text: qsTr("Exit")
          onClicked: {
            ReportHelper.exitReporter();
          }
        }
        Button {
          Layout.preferredWidth: 140
          function restartInSafeMode () {
            ReportHelper.restartApp(true);
            ReportHelper.exitReporter();
          }
          text: qsTr("Restart in Safe Mode")
          // Don't enable restart in safe mode in mac because software rendering is broken
          visible: params.mode === "crash" && Qt.platform.os !== "osx"
          onClicked: {
            params.confirmMessage = qsTr("Safe mode turns off accelerated graphics which can cause stability problems with certain graphics cards or drivers.")
            messageDialog.acceptClicked.disconnect(restartInSafeMode);
            messageDialog.acceptClicked.connect(restartInSafeMode)
          }
        }
        Button {
          text: qsTr("Restart PIA")
          visible: params.mode == "crash"
          onClicked: {
            ReportHelper.restartApp(params.force_safe_mode);
            ReportHelper.exitReporter();
          }
        }
      }
    }
  }
}
