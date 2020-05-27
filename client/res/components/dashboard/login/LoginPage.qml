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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../../javascript/app.js" as App
import "../../daemon"
import "../../theme"
import "../../common"
import "../../core"
import PIA.Error 1.0
import PIA.FlexValidator 1.0
import PIA.BrandHelper 1.0

FocusScope {
  // Whether we display an error, and if we do, what error it is.
  readonly property var errors: {
    'none': 0,
    'auth': 1,
    'rate': 2, // Rate limited
    'api': 3  // Error reaching API, etc. (user's creds might be correct)
  }
  property int shownError: errors.none
  property bool hasValidInput: loginInput.text.length > 0 && passwordInput.text.length > 0
  property bool loginInProgress: false
  readonly property int pageHeight: 400
  readonly property int maxPageHeight: pageHeight

  Rectangle {
    anchors.fill: parent
    color: "transparent"
    clip: true
    LocationMap {
      id: mapImage
      anchors.horizontalCenter: parent.horizontalCenter
      width: 2*height
      // If the dashboard is shorter than expected, reduce the map's height to
      // make up for it
      height: (parent.height >= pageHeight) ? 130 : Math.max(130 + parent.height - pageHeight, 1)
      anchors.top: parent.top
      anchors.topMargin: 36
      mapOpacity: Theme.login.mapOpacity
      markerInnerRadius: 3.5
      markerOuterRadius: 6.5
      location: Daemon.state.vpnLocations.nextLocation
    }

    Text {
      color: Theme.login.errorTextColor
      text: {
        switch(shownError) {
        case errors.auth:
          return uiTr("Invalid login")
        case errors.rate:
          return uiTr("Too many attempts, try again in 1 hour")
        case errors.api:
          return uiTr("Can't reach the server")
        default:
          return ""
        }
      }
      anchors.top: mapImage.bottom
      anchors.topMargin: 10
      anchors.horizontalCenter: parent.horizontalCenter
      font.pixelSize: Theme.login.errorTextPx
      visible: shownError !== errors.none
    }

    FlexValidator {
      id: loginValidator

      // Recent usernames must all be alphanumeric but some historical ones are email addresses
      // As a result of this, let's not validate the login input at all
      // but instead use the validator for convenience as the fixInput() callback allows trimming of input as the user types
      regExp: /.*/
      function fixInput(input) { return input.trim() }
    }

    LoginText {
      id: loginInput
      errorState: shownError !== errors.none
      anchors.horizontalCenter: parent.horizontalCenter
      width: 260
      anchors.top: mapImage.bottom
      anchors.topMargin: 26
      placeholderText: uiTr("Username")
      validator: loginValidator
      onAccepted: parent.login()
      errorTipText: {
        if(loginInput.text.startsWith('x')) {
          //: Shown if the user attempts to login with the wrong account type.
          //: 'p' refers to the letter prefix on the username; the p should be
          //: kept in Latin script.  (Example user names are "p0123456",
          //: "p5858587").
          return uiTr("Use your normal username beginning with 'p'.")
        }
        return ""
      }
    }

    LoginText {
      id: passwordInput
      errorState: shownError !== errors.none
      width: 260
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.top: loginInput.bottom
      anchors.topMargin: 8
      placeholderText: uiTr("Password")
      onAccepted: parent.login()
      masked: true
    }

    LoginButton {
      id: lb
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.top: passwordInput.bottom
      anchors.topMargin: 20
      loginEnabled: hasValidInput
      loginWorking: loginInProgress
      onTriggered: parent.login()
    }

    function login() {
      // The user can press Enter even if the credentials haven't been entered;
      // log in only if the credentials are set.
      if(hasValidInput && !loginInProgress) {
        loginInProgress = true
        shownError = errors.none

        // We trim off any errant newlines from password (usually introduced by copy/paste)
        Daemon.login(loginInput.text, passwordInput.text.replace(/\n+$/, ''), function(error) {
          if (error) {
            // Failure - creds were not valid (or we couldn't communicate
            // with the daemon, etc.)
            loginInProgress = false
            switch(error.code) {
            case NativeError.ApiUnauthorizedError:
              shownError = errors.auth
              break
            case NativeError.ApiRateLimitedError:
              shownError = errors.rate
              break
            default:
              shownError = errors.api
              break
            }
            console.warn('Login failed:', error);
          }
        });
      }
    }

    TextLink {
      text: uiTr("Forgot Password")
      link: BrandHelper.getBrandParam("forgotPasswordLink")

      anchors.bottom: parent.bottom
      anchors.bottomMargin: 24
      anchors.leftMargin: 20
      anchors.left: parent.left
    }

    TextLink {
      text: uiTr("Buy Account")
      link: BrandHelper.getBrandParam("buyAccountLink")
      anchors.bottom: parent.bottom
      anchors.bottomMargin: 24
      anchors.rightMargin: 20
      anchors.right: parent.right
    }
  }

  function resetCreds() {
    loginInput.text = Daemon.account.username
    passwordInput.text = Daemon.account.password
  }

  // If the daemon updates its credentials (mainly for a logout), reset the
  // credentials in the login page
  Connections {
    target: Daemon.account
    onUsernameChanged: resetCreds()
    onPasswordChanged: resetCreds()
  }

  function onEnter () {
    console.log('login onEnter')
    headerBar.logoCentered = false
    headerBar.needsBottomLine = false

    // Clear loginInProgress in case it was set by a prior login (it stays set
    // during the transition)
    loginInProgress = false
    shownError = errors.none
  }

  Component.onCompleted: {
    console.log('login onCompleted')
    // Load the initial stored credentials
    resetCreds()
  }
}
