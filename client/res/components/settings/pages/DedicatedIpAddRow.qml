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
import QtQuick.Window 2.3
import "../../core"
import "../../theme"
import "../../common"
import "../../client"
import "../../daemon"
import "../inputs"
import PIA.Error 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util

DedicatedIpRowBase {
  id: addRow

  implicitHeight: 35

  readonly property string displayName: uiTr("Activate Dedicated IP")

  // This row only has one column
  effectiveColumnFor: function(column){return keyColumns.region}
  keyboardSelect: function(column){addDipDialog.show()}
  accRow: NativeAcc.TableRow {
    name: displayName
    item: addRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }
  // Cell annotations
  accRegionCell: NativeAcc.TableCellButton {
    name: displayName
    item: addText
    onActivated: addRow.clicked()
  }
  accIpCell: null
  accRemoveCell: null

  function clicked() {
    addRow.focusCell(0)
    addDipDialog.show()
  }

  OverlayDialog {
    id: addDipDialog
    buttons: [
      // The OK button doesn't implicitly close this dialog, because it has to
      // send an RPC to validate the token
      { code: Dialog.Ok, clicked: function(){addDipDialog.dialogAccept()}, suppressDefault: true },
      Dialog.Cancel
    ]
    canAccept: activateDipToken.text.length > 0 && requestState !== requestStates.processing
    canReject: requestState !== requestStates.processing
    contentWidth: 300
    title: addRow.displayName

    readonly property var requestStates: ({
      idle: 0,  // No request occurring yet
      processing: 1,  // Request is in process (UI disabled)
      invalid: 2, // DIP token was invalid
      expired: 3, // DIP token was expired
      otherError: 4 // Some other error occurred
    })

    property int requestState: requestStates.idle

    function dialogAccept() {
      requestState = requestStates.processing
      Daemon.addDedicatedIp(activateDipToken.text, function(error){
        if(error) {
          switch(error.code) {
            case NativeError.DaemonRPCDedicatedIpTokenExpired:
              requestState = requestStates.expired
              break
            case NativeError.DaemonRPCDedicatedIpTokenInvalid:
              requestState = requestStates.invalid
              break
            default:
              requestState = requestStates.otherError
              break
          }
        }
        else {
          requestState = requestStates.idle
          addDipDialog.accept()
        }
      })
    }

    ColumnLayout {
      width: parent.width

      InputLabel {
        id: dipTokenLabel
        text: uiTr("Dedicated IP Token")
      }

      OneLinkMessage {
        id: activateDipDesc
        Layout.fillWidth: true
        color: Theme.settings.inputDescriptionColor
        linkColor: Theme.settings.inputDescLinkColor
        //: The [[double square brackets]] are formatted as a link.  Please
        //: mark the corresponding translated text with double square brackets
        //: so the link will be applied correctly.
        text: uiTr("Paste your token below.  If you've recently purchased a dedicated IP, you can generate the token by going to the [[My Account]] page.")
        onLinkActivated: {
          Qt.openUrlExternally("https://www.privateinternetaccess.com/pages/client-control-panel/dedicated-ip")
        }
      }

      // Spacer
      Item {
        width: 1
        height: 3
      }

      SettingsTextField {
        id: activateDipToken
        label: dipTokenLabel.text
        Layout.fillWidth: true
        enabled: addDipDialog.requestState !== addDipDialog.requestStates.processing

        focus: true

        borderColor: {
          switch(addDipDialog.requestState)
          {
            case addDipDialog.requestStates.otherError:
            case addDipDialog.requestStates.invalid:
            case addDipDialog.requestStates.expired:
              return Theme.settings.inputTextboxWarningBorderColor
          }
          // Although the user can't click OK when the text box is empty, don't
          // use the red border here.  It doesn't add any value since this
          // dialog only has one field, the only invalid state is "empty", and
          // it's obvious that OK is disabled.

          return defaultBorderColor
        }

        onAccepted: addDipDialog.dialogAccept()
        onTextEdited: addDipDialog.requestState = addDipDialog.requestStates.idle
      }

      DialogMessage {
        id: expiredMessage
        Layout.fillWidth: true
        visible: addDipDialog.requestState === addDipDialog.requestStates.expired
        icon: 'warning'
        text: {
          return "<b><font color=\"" +
            Util.colorToStr(Theme.dashboard.notificationWarningLinkColor) + "\">" +
            //: Shown when the user attempts to add a Dedicated IP token that is
            //: already expired.
            uiTr("Your token has expired.") + "<br></font></b>" +
            uiTr("You can purchase a new one from the My Account page.")
        }
      }

      DialogMessage {
        id: invalidMessage
        Layout.fillWidth: true
        visible: addDipDialog.requestState === addDipDialog.requestStates.invalid
        icon: 'warning'
        text: {
          return "<b><font color=\"" +
            Util.colorToStr(Theme.dashboard.notificationWarningLinkColor) + "\">" +
            uiTr("Your token is invalid.") + "<br></font></b>" +
            uiTr("Make sure you have entered the token correctly.")
        }
      }

      DialogMessage {
        id: otherErrorMessage
        Layout.fillWidth: true
        visible: addDipDialog.requestState === addDipDialog.requestStates.otherError
        icon: 'warning'
        text: {
          return "<b><font color=\"" +
            Util.colorToStr(Theme.dashboard.notificationWarningLinkColor) + "\">" +
            uiTr("Couldn't check the token.") + "<br></font></b>" +
            uiTr("Can't reach the server to check the token.  Please try again later.")
        }
      }
    }

    function show() {
      requestState = requestStates.idle
      activateDipToken.focus = true
      activateDipToken.text = ""
      visible = true
      focus = true
      open()
    }
  }

  Image {
    height: 15
    width: 15

    source: Theme.settings.splitTunnelAddApplicationButtonHover
    anchors.verticalCenter: parent.verticalCenter
    anchors.left: parent.left
    anchors.leftMargin: 11
  }

  Text {
    id: addText
    text: addRow.displayName
    color: Theme.settings.inputListItemPrimaryTextColor
    font.pixelSize: 12
    x: 40
    anchors.verticalCenter: parent.verticalCenter
  }

  MouseArea {
    id: addApplicationMouseArea
    anchors.fill: parent
    cursorShape: Qt.PointingHandCursor
    hoverEnabled: true
    onClicked: addRow.clicked()
  }

  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn >= 0 // There's only one cell in this row
    inside: true
  }
}
