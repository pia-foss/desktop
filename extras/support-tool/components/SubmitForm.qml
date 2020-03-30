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

import QtQuick 2.0
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import PIA.ReportHelper 1.0
import PIA.PayloadBuilder 1.0

Item {

  function payloadIsAvailable() {
    // do we have any crash dumps to send?
    if (params.include_crash && (params.daemon_crash_files.length > 0 || params.client_crash_files.length > 0))
      return true

    // do we have any logs to send?
    if (params.include_logs && params.logs.length > 0)
      return true

    // any other kinds of files we're sending? (currently just diagnostics)
    return params.files.length > 0
  }

  function trySendPayload () {
      retryCount ++;
      ReportHelper.sendPayload(PayloadBuilder.payloadZipContent(),
                               comments.text)
      formStatus = 1
  }

  // 0 - Normal
  // 1 - Sending
  // 2 - Success
  // -1 - error
  property int formStatus: 0
  property string referenceId: ""
  property string networkErrorMessage: ""
  property int retryCount: 0

  readonly property int page_start: 0
  readonly property int page_legal: 0
  readonly property int page_submitted: 0

  // Number of times to retry submitting payload if
  // upload fails
  readonly property int retryLimit: 3

  // Milliseconds to wait between retries to submit report
  readonly property int retryDelay: 500
  StackLayout {
    id: wizardLayout
    currentIndex: 0
    anchors.fill: parent
    ColumnLayout {
      Layout.fillHeight: true
      Layout.fillWidth: true
      spacing: 0
      Text {
        text: "You can send an error report which will help identify the issue. "
        wrapMode: Text.Wrap
        Layout.fillWidth: true
      }
      CheckBox {
        Layout.topMargin: 3
        text: "Include technical debug logs."
        checked: params.include_logs
        enabled: formStatus <= 0
        onClicked: {
          params.include_logs = !params.include_logs
        }
      }

      Rectangle {
        color: "#f5515f"
        Layout.fillWidth: true
        visible: params.include_logs && params.logs.length === 0
        height: warningText.contentHeight + 25
        Text {
          id: warningText
          anchors.fill: parent
          anchors.margins: 5
          wrapMode: Text.WordWrap
          color: "#fff"
          text: "Warning: No debug logs were found. Please ensure debug mode is enabled in the application settings."
        }
      }

      CheckBox {
        Layout.topMargin: 2
        visible: params.daemon_crash_files.length > 0
                 || params.client_crash_files.length > 0
        text: "Include crash reports"
        checked: params.include_crash
        enabled: formStatus <= 0
        onClicked: {
          params.include_crash = !params.include_crash
        }
      }

      Rectangle {
        Layout.topMargin: 8
        Layout.fillWidth: true
        border.color: "#5c6370"
        color: "#eeeeee"
        Layout.preferredHeight: comments.contentHeight + 20
        Layout.maximumHeight: 110
        ScrollView {
          anchors.fill: parent
          TextArea {
            property int charLimit: 1000
            id: comments
            enabled: formStatus <= 0
            selectByMouse: true
            wrapMode: TextEdit.WordWrap
            placeholderText: qsTr("(Optional) Additional Information")
            onTextChanged: {
              if (text.length > charLimit) {
                remove(charLimit, text.length)
              }
            }
          }
        }
      }

      Rectangle {
        Layout.topMargin: 10
        color: "#5f51f5"
        Layout.fillWidth: true
        visible: !payloadIsAvailable()
        height: noPayloadWarning.contentHeight + 25
        Text {
          id: noPayloadWarning
          anchors.fill: parent
          anchors.margins: 5
          wrapMode: Text.WordWrap
          color: "#fff"
          text: "Error: No diagnostic info available. Please ensure debug mode is enabled in the application settings."
        }
      }

      RowLayout {
        Layout.topMargin: 10
        Text {
          id: statusText
          text: {
            switch (formStatus) {
            case 0:
              return (comments.charLimit - comments.text.length).toString(
                    ) + " characters remaining"
            }
            return "";
          }
          Layout.fillWidth: true
        }
        Button {
          text: "Next"
          enabled: formStatus <= 0 && payloadIsAvailable()
          onClicked: {
            wizardLayout.currentIndex = 1
          }
        }
      }

      Item {
        // spacer
        Layout.fillHeight: true
        Layout.fillWidth: true
      }
      Text {
        text: "We value your privacy. No identifying information or traffic/website usage information is sent. Error reports are not sent to any third party."
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        color: "#5c6370"
        font.pixelSize: 11
      }
    }

    //
    //
    // Step 2: The "legal" page
    //
    //
    ColumnLayout {
      Layout.fillHeight: true
      Layout.fillWidth: true

      Text {
        text: "Terms:"
      }

      Rectangle {
        border.color: "#89909a"
        Layout.fillWidth: true
        Layout.preferredHeight: 200

        ScrollView {
          padding: 5
          anchors.fill: parent
          contentWidth: parent.width - 2 * padding
          contentHeight: legalText.implicitHeight + 2 * padding
          clip: true

          Text {
            id: legalText
            width: parent.width
            wrapMode: Text.WordWrap
            text: "By submitting this debug file, you understand that the file may contain personal information. If necessary, any personal information contained within the file will be processed on a legitimate basis consistent with Article 6(1) of the GDPR. By submitting this file, I expressly consent to the processing of my personal information for legitimate interests associated with the ongoing business operations of the data processor."
          }
        }
      }



      RowLayout {
        Text {
          text: "< Back"
          font.underline: true
          color: "#037900"
          visible: formStatus <= 0
          MouseArea {
            anchors.fill: parent
            onClicked: {
              wizardLayout.currentIndex = 0;
            }
          }
        }

        Item {
            Layout.fillWidth: true
        }
        CheckBox {
          id: legalCheckbox
          text: "I Agree"
        }

        Button {
            text: "Send"
            enabled: formStatus <= 0 && legalCheckbox.checked
            onClicked: {
                // create the payload and send if successful
                if (makePayload()) {
                    retryCount = 0;
                    trySendPayload ();
                } else {
                  formStatus = -1
                }
            }
        }
      }
      Text {
        text: {
            switch (formStatus) {
            case 1:
                return "Sending your report. Please wait"
            case -1:
              // Show a network error message if one is set.
                return "Encountered an error. Please try again." + (
                  networkErrorMessage.length > 0 ?
                    "\n("+ networkErrorMessage + ")"
                  : "")
            }
            return "";
        }
        Layout.alignment: Qt.AlignRight
      }

      Item {
        // spacer
        Layout.fillHeight: true
        Layout.fillWidth: true
      }
    }

    ColumnLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      Text {
          text: "Your report has been submitted successfully. Thank you."
      }
      RowLayout {
        Layout.topMargin: 10
        visible: true
        Layout.fillWidth: true

        Text {
          text: "Reference ID: "
          Layout.rightMargin: 5
        }

        TextField {
          id: reportId
          text: referenceId
          height: 15
          width: 100
          enabled: false
        }
        Text {
          text: "Copy"
          color: "#4cb649"
          opacity: 0.7
          font.underline: true
          MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
              reportId.selectAll()
              reportId.copy()
              parent.text = "Copied"
            }
          }
        }
      }

      Connections {
        target: ReportHelper
        onUploadSuccess: function (shortcode) {
          referenceId = shortcode
          formStatus = 2
          wizardLayout.currentIndex = 2
        }
        onUploadFail: function (msg) {
          if(retryCount <= retryLimit) {
              // Wait a little while and retry sending payload
              console.log("Retrying on failure");
              retryTimer.start();
          } else {
              networkErrorMessage = msg
              formStatus = -1
          }
        }
      }

      Timer {
          id: retryTimer
          interval: retryDelay
          onTriggered: {
              trySendPayload();
          }
      }

      Item {
        // spacer
        Layout.fillHeight: true
        Layout.fillWidth: true
      }
    }
  }
}
