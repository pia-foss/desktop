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

import QtQuick 2.11
import QtQuick.Window 2.11
import QtQuick.Controls 2.2
import PIA.ReportHelper 1.0
import PIA.PayloadBuilder 1.0
import "./"

Window {
  id: rootWindow
  visible: true
  width: 700
  height: 500
  title: {
    if (params.mode == 'log')
      return qsTr(ReportHelper.brandName + ": Debug Logs")
    if (params.mode == 'crash')
      return qsTr(ReportHelper.brandName + ": Error")
    return qsTr(ReportHelper.brandName)
  }
  color: "#efefef"

  function makePayload(writeToFile) {
    if (typeof (writeToFile) === "undefined")
      writeToFile = ""

    // Create a new payload
    PayloadBuilder.start()

    params.files.forEach(function (filePath) {
      PayloadBuilder.addFile(filePath)
    })

    if (params.include_logs) {
      params.logs.forEach(function (filePath) {
        PayloadBuilder.addLogFile(filePath)
      })
    }

    if (params.include_crash) {
      params.daemon_crash_files.forEach(function (filePath) {
        PayloadBuilder.addDaemonDumpFile(filePath)
      })
      params.client_crash_files.forEach(function (filePath) {
        PayloadBuilder.addClientDumpFile(filePath)
      })
    }

    return PayloadBuilder.finish(writeToFile)
  }

  QtObject {
    id: params
    objectName: "params"
    property string mode: "logs"
    property var logs: []
    property var files: []
    property bool force_safe_mode: {
      return ReportHelper.checkCrashesForBlacklist(params.client_crash_files);
    }

    property string daemon_crash: ""
    property string client_crash: ""
    property int invoke_pipe: -1
    readonly property var client_crash_files: {
      if (client_crash == "")
        return []
      return ReportHelper.findCrashFiles(client_crash)
    }
    readonly property var daemon_crash_files: {
      if (daemon_crash == "")
        return []
      return ReportHelper.findCrashFiles(daemon_crash)
    }

    property string version: ""
    property bool include_logs: true
    property bool include_crash: true
    property string errorMessage: ""
    property string confirmMessage: ""
  }

  SupportToolUI {
    anchors.fill: parent
  }

  Item {
    id: errorDialog
    visible: params.errorMessage.length > 0
    anchors.fill: parent
    Rectangle {
      color: "#333"
      opacity: 0.3
      anchors.fill: parent
      MouseArea {
        anchors.fill: parent
        onClicked: {

        }
      }
    }

    Rectangle {
      color: "#fff"
      border.color: "#333"
      anchors.centerIn: parent
      width: 300
      height: 200

      Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 15
        wrapMode: Text.WordWrap
        text: params.errorMessage
      }

      Button {
        text: "Ok"
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 15

        onClicked: {
          params.errorMessage = "";
        }
      }
    }
  }

  Item {
    id: messageDialog
    visible: params.confirmMessage.length > 0
    anchors.fill: parent
    signal acceptClicked();
    Rectangle {
      color: "#333"
      opacity: 0.3
      anchors.fill: parent
      MouseArea {
        anchors.fill: parent
        onClicked: {

        }
      }
    }

    Rectangle {
      color: "#fff"
      border.color: "#333"
      anchors.centerIn: parent
      width: 300
      height: 200

      Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 15
        wrapMode: Text.WordWrap
        text: params.confirmMessage
      }

      Button {
        text: "Ok"
        id: okButton
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 15

        onClicked: {
          params.confirmMessage = "";
          messageDialog.acceptClicked();
        }
      }
      Button {
        text: "Cancel"
        anchors.bottom: parent.bottom
        anchors.right: okButton.left
        anchors.margins: 15

        onClicked: {
          params.confirmMessage = "";
        }
      }
    }
  }
}
