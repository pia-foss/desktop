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
import QtQuick.Dialogs 1.3

Item {
  height: fileListLayout.height
  Column {
    id: fileListLayout
    width: parent.width
    spacing: 0

    // File dialog for saving the zip file containing the payload
    FileDialog {
        id: fileDialog
        title: "Save zip file"
        folder: shortcuts.home
        selectExisting: false
        defaultSuffix: "zip"
        onAccepted: {
          // The path the user selects
          var path = fileDialog.fileUrl;
          if(makePayload(path)) {
            ReportHelper.showFileInSystemViewer(path);
          }
          else {
            // TODO: Show error to user.
            console.log("Warning: Could not save payload");
          }
        }
        onRejected: {
            console.log("Canceled")
        }
    }

    Rectangle {
      border.color: "#889099"
      height: 30
      width: parent.width
      Text {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.topMargin: 5
        text: "Files"
        font.pixelSize: 13
      }

      Text {
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.topMargin: 7
        anchors.top: parent.top
        text: "Save as Zip"
        font.pixelSize: 10
        font.underline: true
        color: "#037900"
        MouseArea {
          anchors.fill: parent
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            fileDialog.open();
          }
        }
      }
    }
    Rectangle {
      id: fileListContainer
      opacity: 0.8
      width: parent.width
      height: fileList.height
      border.color: "#d7d8d9"
      ListView {
        x: 0
        y: 0
        width: parent.width
        height: model.length * 25
        id: fileList
        Component {
          id: fileItemDelegate
          Rectangle {
            height: 25
            width: fileList.width
            Rectangle {
              height: 1
              width: parent.width
              x:0
              y:0
              color: "#d7d8d9"
              opacity: 0.7
            }

            Text {
              // File name
              x: 5
              y: 3
              text: ReportHelper.getFileName(modelData);
              color: "#2b2e39"
              width: 200
              height: 25
              elide: Text.ElideMiddle
              font.pixelSize: 12
            }
            Text {
              anchors.right: parent.right
              anchors.rightMargin: 10
              anchors.top: parent.top
              anchors.topMargin: 7
              text: "Open"
              color: "#4cb649"
              opacity: 0.7
              font.underline: true
              font.pixelSize: 10
              MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: ReportHelper.showFileInSystemViewer(modelData)
              }
            }
          }
        }
        delegate: fileItemDelegate

        model: {
          var result = [];

          result = result.concat(params.files);
          if(params.include_logs) {
            result = result.concat(params.logs);
          }
          if(params.include_crash) {
            result = result.concat(params.client_crash_files);
            result = result.concat(params.daemon_crash_files);
          }

          return result;
        }
      }
    }
  }
}
