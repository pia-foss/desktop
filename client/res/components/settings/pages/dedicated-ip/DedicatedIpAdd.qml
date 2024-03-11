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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

import "../../../core"
import "../../../theme"
import "../../../common"
import "../../../client"
import "../../../daemon"
import "../../inputs"
import "../"
import PIA.Error 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util

//import QtQuick.Window 2.10 as QtQuickWindow

Item {
  id: addDip
  property int requestState: requestStates.idle

  readonly property var requestStates: ({
    idle: 0,  // No request occurring yet
    processing: 1,  // Request is in process (UI disabled)
    invalid: 2, // DIP token was invalid
    expired: 3, // DIP token was expired
    otherError: 4, // Some other error occurred
    rateLimited: 5 // Rate limited [ Error 429 ]
  })

  readonly property bool isErrorState: requestState !== requestStates.idle && requestState !== requestStates.processing

  readonly property int errorHeight: expiredMessage.visible ? expiredMessage.implicitHeight : 0 +
            invalidMessage.visible ? invalidMessage.implicitHeight : 0 +
            otherErrorMessage.visible ? otherErrorMessage.implicitHeight : 0 +
            rateLimitedMessage.visible ? rateLimitedMessage.implicitHeight : 0
  readonly property int messageHeight: activateDipDesc.implicitHeight

  property real retryAfterTime: 0

  // The current time which can be updated by a timer
  property real currentTime: 0

  // A timer that always updates the current time once every second
  // because we cannot have the current time automatically updated
  Timer {
    onTriggered: {
      currentTime = Date.now();
    }
    repeat: true
    interval: 1000
    running: retryAfterTime > 0 && retryAfterTime + 2000> currentTime
  }



  function dialogAccept() {
    requestState = requestStates.processing
    retryAfterTime = 0
    currentTime = 0
    Daemon.addDedicatedIp(activateDipToken.text, function(error){
      if(error) {
        switch(error.code) {
        case NativeError.DaemonRPCDedicatedIpTokenExpired:
          requestState = requestStates.expired
          break
        case NativeError.DaemonRPCDedicatedIpTokenInvalid:
          requestState = requestStates.invalid
          break
        case NativeError.ApiRateLimitedError:
          requestState = requestStates.rateLimited
          currentTime = Date.now()
          retryAfterTime = error.retryAfterTime

          break
        default:
          requestState = requestStates.otherError
          break
        }
      }
      else {
        requestState = requestStates.idle
      }
    })
  }

  function clearText () {
    activateDipToken.text = "";
  }



  ColumnLayout {
    anchors.fill: parent
    StaticText {
      color: Theme.dashboard.textColor
      text: uiTr("Activate Your Dedicated IP")
    }
    OneLinkMessage {
      Layout.fillWidth: true
      id: activateDipDesc
      color: Theme.settings.inputDescriptionColor
      linkColor: Theme.settings.inputDescLinkColor
      //: The [[double square brackets]] are formatted as a link.  Please
      //: mark the corresponding translated text with double square brackets
      //: so the link will be applied correctly.
      text: uiTranslate("DedicatedIpAddRow", "Paste your token below.  If you've recently purchased a dedicated IP, you can generate the token by going to the [[My Account]] page.")
      onLinkActivated: {
        Qt.openUrlExternally("https://www.privateinternetaccess.com/pages/client-control-panel/dedicated-ip")
      }
    }

    // Spacer
    Item {
      width: 1
      height: 3
    }

    Item {
      id:tokenInputWrapper
      Layout.fillWidth: true
      Layout.preferredHeight: activateDipToken.implicitHeight


      SettingsTextField {
        readonly property real rtlFlipValue: Window.window ? Window.window.rtlFlip : 1
        readonly property bool rtlFlipFlag: rtlFlipValue === -1
        anchors.fill: parent
        id: activateDipToken
        label: uiTranslate("DedicatedIpAddRow", "Dedicated IP Token")
        enabled: addDip.requestState !== addDip.requestStates.processing
        textBoxVerticalPadding: 15

        // On RTL languages like Arabic, paddings are not automatically flipped. In order to accomodate the Activate
        // button, we add extra padding to elide text, but also present a regular text field for input.
        //
        //
        leftPadding: rtlFlipFlag ? activateButton.width + 20 : 15
        rightPadding: rtlFlipFlag ? 15 : activateButton.width + 20
        placeholderText: uiTr("Paste in your token here")

        focus: true

        borderColor: {
          switch(addDip.requestState)
          {
          case addDip.requestStates.otherError:
          case addDip.requestStates.invalid:
          case addDip.requestStates.rateLimited:
          case addDip.requestStates.expired:
            return Theme.settings.inputTextboxWarningBorderColor
          }
          // Although the user can't click OK when the text box is empty, don't
          // use the red border here.  It doesn't add any value since this
          // dialog only has one field, the only invalid state is "empty", and
          // it's obvious that OK is disabled.

          return defaultBorderColor
        }

        onAccepted: addDip.dialogAccept()
        onTextEdited: addDip.requestState = addDip.requestStates.idle

      }

      SettingsButton{
        id: activateButton
        text: uiTr("Activate")
        anchors.right: activateDipToken.right
        anchors.verticalCenter: activateDipToken.verticalCenter
        enabled: activateDipToken.text.length > 0 && requestState !== requestStates.processing

        anchors.rightMargin: 10
        onClicked: {
          addDip.dialogAccept();
        }
      }
    }





    DialogMessage {
      id: expiredMessage
      Layout.fillWidth: true
      visible: addDip.requestState === addDip.requestStates.expired
      icon: 'warning'
      text: {
        return "<b><font color=\"" +
            Util.colorToStr(Theme.dashboard.notificationWarningLinkColor) + "\">" +
            //: Shown when the user attempts to add a Dedicated IP token that is
            //: already expired.
            uiTranslate("DedicatedIpAddRow", "Your token has expired.") + "<br></font></b>" +
            uiTranslate("DedicatedIpAddRow", "You can purchase a new one from the My Account page.")
      }
    }

    DialogMessage {
      id: invalidMessage
      Layout.fillWidth: true
      visible: addDip.requestState === addDip.requestStates.invalid
      icon: 'warning'
      text: {
        return "<b><font color=\"" +
            Util.colorToStr(Theme.dashboard.notificationWarningLinkColor) + "\">" +
            uiTranslate("DedicatedIpAddRow", "Your token is invalid.") + "<br></font></b>" +
            uiTranslate("DedicatedIpAddRow", "Make sure you have entered the token correctly.")
      }
    }

    DialogMessage {
      id: rateLimitedMessage
      Layout.fillWidth: true
      visible: addDip.requestState === addDip.requestStates.rateLimited
      icon: 'warning'
      text: {
        return "<b><font color=\"" +
            Util.colorToStr(Theme.dashboard.notificationWarningLinkColor) + "\">" +
            uiTranslate("DedicatedIpAddRow", "Too many attempts.") + "<br></font></b>" +
            Messages.tryAgainMessage(Math.ceil((retryAfterTime - currentTime) / 1000))
      }
    }


    DialogMessage {
      id: otherErrorMessage
      Layout.fillWidth: true
      visible: addDip.requestState === addDip.requestStates.otherError
      icon: 'warning'
      text: {
        return "<b><font color=\"" +
            Util.colorToStr(Theme.dashboard.notificationWarningLinkColor) + "\">" +
            uiTranslate("DedicatedIpAddRow", "Couldn't check the token.") + "<br></font></b>" +
            uiTranslate("DedicatedIpAddRow", "Can't reach the server to check the token.  Please try again later.")
      }
    }

    Item {
      Layout.fillHeight: true
      Layout.fillWidth: true
    }
  }
}
