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
import QtQuick.Window 2.11
import "../daemon"
import "../theme"
import "../client"
import "../common"
import "../core"
import "../helpers"
import "qrc:/javascript/util.js" as Util
import PIA.NativeHelpers 1.0
import PIA.BrandHelper 1.0
import PIA.NativeAcc 1.0 as NativeAcc

Item {
  id: headerBar

  property var pm
  function setPageManager(_pm) {
    pm = _pm
  }

  property bool needsBottomLine: true
  property bool needsMenuButton: true
  property bool logoCentered: false
  // Description of back button used for screen readers (undefined if none)
  property var backButtonDescription
  // Function called when back button clicked - if null, the back button is not
  // shown.
  property var backButtonFunction
  readonly property var imageStates: {
    'none': 0,
    'yellow': 1,
    'green': 2,
    'red': 3
  }
  property int activeImage: {
    // Errors and warnings take precedence over the connected state.
    if(ClientNotifications.firstErrorTitle)
      return imageStates.red
    if(ClientNotifications.firstWarningTitle)
      return imageStates.yellow

    // If there are no errors/warnings, and we're connected, show the green
    // header.
    if(connState.connectionState === connState.stateConnected)
      return imageStates.green

    // Otherwise, plain header.
    return imageStates.none
  }

  // the menu buttons (arrow and 3 dots) need to have a lighter/darker variant
  // depending on the current header bar image (colored headers)
  property bool darkButtons: activeImage !== imageStates.none

  property real yellowOpacity: activeImage === imageStates.yellow ? 1.0 : 0.0
  property real greenOpacity: activeImage === imageStates.green ? 1.0 : 0.0
  property real redOpacity: activeImage === imageStates.red ? 1.0 : 0.0

  function colorWithAlpha(color, alpha) {
    return Qt.rgba(color.r, color.g, color.b, alpha)
  }

  function composeHeaderColor(yellow, green, red) {
    var result = Theme.dashboard.backgroundColor
    result = Qt.tint(result, colorWithAlpha(yellow, yellowOpacity))
    result = Qt.tint(result, colorWithAlpha(green, greenOpacity))
    result = Qt.tint(result, colorWithAlpha(red, redOpacity))
    return result
  }

  // Show the "background" elements when we're not showing any colored header.
  property real backgroundElementOpacity: activeImage === imageStates.none ? 1 : 0
  // Colored header top/bottom colors
  property color topColor: composeHeaderColor(Theme.dashboard.headerYellowColor,
                                              Theme.dashboard.headerGreenColor,
                                              Theme.dashboard.headerRedColor)
  property color bottomColor: composeHeaderColor(Theme.dashboard.headerYellowBottomColor,
                                                 Theme.dashboard.headerGreenBottomColor,
                                                 Theme.dashboard.headerRedBottomColor)

  Behavior on backgroundElementOpacity {
    NumberAnimation {
      duration: Theme.animation.normalDuration
    }
    enabled: headerBar.Window.window.visible
  }
  // Animate the individual Y/G/R opacities instead of animating the composed
  // top/bottom colors.
  // - Theme toggles *don't* animate, the background color changes immediately,
  //   even if a color blend animation is occurring
  Behavior on yellowOpacity {
    NumberAnimation {
      duration: Theme.animation.normalDuration
    }
    // If the dashboard is not visible, disable this behavior.
    //
    // For whatever reason, QML does not seem to process property changes while
    // the window is hidden, which means that if a state change occurs while the
    // window is hidden, the animation would trigger when it is next shown.  This
    // is strange if the dashboard is hidden in the connecting/disconnecting state
    // (yellow header), then shown in a different state - the yellow header
    // briefly appears and fades away.
    enabled: headerBar.Window.window.visible
  }
  Behavior on greenOpacity {
    NumberAnimation {
      duration: Theme.animation.normalDuration
    }
    enabled: headerBar.Window.window.visible
  }
  Behavior on redOpacity {
    NumberAnimation {
      duration: Theme.animation.normalDuration
    }
    enabled: headerBar.Window.window.visible
  }

  ConnStateHelper {
    id: connState
  }

  HeaderGradient {
    id: gradient
    anchors.fill: parent
    topRadius: contentRadius
    topColor: headerBar.topColor
    bottomColor: headerBar.bottomColor
  }

  // The prerelease "overlay" actually stacks underneath the "back" button so
  // its focus cue appears on top.  The graphic for the "back" button doesn't
  // overlap the prerelease overlay graphic, so this isn't noticeable otherwise.
  StaticImage {
    x: 0
    y: 0
    width: 45
    height: 45
    visible: Client.features.prerelease
    readonly property bool beta: Client.features.beta
    label: {
      if(beta)
        return Messages.betaPrereleaseImg
      else
        return Messages.alphaPrereleaseImg
    }
    source: beta ? Theme.dashboard.headerBetaImage : Theme.dashboard.headerAlphaImage

    // This image can't be flipped for RTL because it contains text, but
    // fortunately rotating it 90 degrees lets it match up with the opposite
    // corner.
    rotation: Client.state.activeLanguage.rtlMirror ? 90 : 0
  }

  // Back button
  Item {
    id: backButton
    visible: !!backButtonFunction
    anchors.verticalCenter: parent.verticalCenter
    anchors.left: parent.left
    anchors.leftMargin: 12
    height: 32
    width: 32

    Image {
      source: darkButtons ? Theme.dashboard.headerBack2Image : Theme.dashboard.headerBackImage
      height: sourceSize.height / 2
      width: sourceSize.width / 2
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.verticalCenter: parent.verticalCenter
      rtlMirror: true
    }

    ButtonArea {
      anchors.fill: parent
      cursorShape: Qt.PointingHandCursor
      focusCueColor: darkButtons ? Theme.popup.focusCueDarkColor : undefined

      //: Screen reader annotation for the "Back" button in the header, which
      //: returns to the previous page.  Should use the typical term for a
      //: "back" button in a dialog flow, wizard, etc.
      name: uiTr("Back")
      description: backButtonDescription

      onClicked: {
        if (backButtonFunction) {
          backButtonFunction()
        }
      }
    }
  }

  // The logo and bottom line are faded out when any colored header appears -
  // these items appear to be "in the background" under the colored overlay
  // (even though the color is not actually blended as an overlay to avoid
  // artifacts).
  Item {
    id: backgroundElementWrapper
    anchors.fill: parent
    opacity: backgroundElementOpacity

    StaticImage {
      id: logoImg
      source: Theme.dashboard.headerLogoImage
      label: NativeHelpers.productName
      height: logoCentered ? 22.67 : 28
      width: logoCentered ? 160 : 200
      visible: title.text === ""
      anchors.horizontalCenter: parent.horizontalCenter
      y: logoCentered ? 19 : 32
      Behavior on y {
        SmoothedAnimation {
          duration: Theme.animation.normalDuration
        }
        enabled: headerBar.Window.window.visible
      }
      Behavior on height {
        SmoothedAnimation {
          duration: Theme.animation.normalDuration
        }
        enabled: headerBar.Window.window.visible
      }
      Behavior on width {
        SmoothedAnimation {
          duration: Theme.animation.normalDuration
        }
        enabled: headerBar.Window.window.visible
      }
    }

    Rectangle {
      color: Theme.dashboard.headerBottomLineColor
      height: 1
      visible: needsBottomLine
      anchors.bottom: parent.bottom
      anchors.left: parent.left
      anchors.right: parent.right
    }
  }

  // Status label (replaces logo image).  Needs to stack right after the logo
  // image for proper control ordering with screen readers.
  StaticText {
    id: title
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.topMargin: 8
    anchors.bottomMargin: 8
    anchors.horizontalCenter: parent.horizontalCenter
    width: 210
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    wrapMode: Text.Wrap
    font.pixelSize: Theme.dashboard.headerTextPx
    text: {
      if(ClientNotifications.firstErrorTitle)
        return ClientNotifications.firstErrorTitle
      if(ClientNotifications.firstWarningTitle)
        return ClientNotifications.firstWarningTitle
      if(ClientNotifications.firstInfoTitle)
        return ClientNotifications.firstInfoTitle

      if(connState.snoozeModeEnabled) {
        switch (connState.snoozeState) {
        case connState.snoozeConnecting:
          return uiTr("RESUMING")
        case connState.snoozeDisconnecting:
          return uiTr("SNOOZING")
        case connState.snoozeDisconnected:
          return uiTr("SNOOZED")
        default:
          return "";
        }
      }

      switch (connState.connectionState) {
        default:
        case connState.stateDisconnected:
          return ""
        case connState.stateConnecting:
          return uiTr("CONNECTING")
        case connState.stateDisconnecting:
          return uiTr("DISCONNECTING")
        case connState.stateConnected:
          return uiTr("CONNECTED")
      }
    }
    color: {
      switch(activeImage) {
      default:
      case imageStates.none:
        return Theme.dashboard.headerDefaultTextColor
      case imageStates.yellow:
        return Theme.dashboard.headerYellowTextColor
      case imageStates.green:
        return Theme.dashboard.headerGreenTextColor
      case imageStates.red:
        return Theme.dashboard.headerRedTextColor
      }
    }
    Behavior on color {
      ColorAnimation {
        duration: Theme.animation.normalDuration
      }
    }
  }

  Item {
    height: 30
    width: 30
    anchors.verticalCenter: parent.verticalCenter
    anchors.right: parent.right
    anchors.rightMargin: 9

    Image {
      id: menuImage2
      source: darkButtons ? Theme.dashboard.headerMenuDarkImage : Theme.dashboard.headerMenuLightImage
      anchors.verticalCenter: parent.verticalCenter
      anchors.horizontalCenter: parent.horizontalCenter
      height: sourceSize.height / 2
      width: sourceSize.width / 2
    }

    Image {
      id: updateBadge
      source: darkButtons ? Theme.dashboard.headerMenuUpdateDarkImage : Theme.dashboard.headerMenuUpdateLightImage
      visible: ClientNotifications.updateAvailable.showInMenus
      x: parent.width / 2
      y: 0
      width: sourceSize.width / 2
      height: sourceSize.height / 2
    }

    // The menu button - a generic button area, has a specific MenuButton annotation
    GenericButtonArea {
      anchors.fill: parent

      NativeAcc.MenuButton.name: {
        if(!ClientNotifications.updateAvailable.showInMenus) {
          //: Screen reader annotation for the "Menu" button in the header.
          //: This button displays a popup menu.
          return uiTr("Menu")
        }
        //: Screen reader annotation for the "Menu" button in the header when it
        //: displays the "update available" badge.  The button still displays
        //: the normal popup menu, and the "Menu" translation should come first
        //: since that's its action.  "Update available" is added as an
        //: additional description of the update badge.
        return uiTr("Menu, update available")
      }
      NativeAcc.MenuButton.onActivated: mouseClicked()

      cursorShape: Qt.PointingHandCursor
      // Don't accept any buttons when the menu is already opened.  This ensures
      // that clicking the dots a second time closes the menu without reopening
      // it.
      //
      // Checking optionsMenu.opened in onClicked has no effect - by that point,
      // the menu already dismissed itself when the mouse button was initially
      // pressed.  onClicked fires when the button is released, so we would
      // always re-show the popup menu.
      //
      // Accepting no buttons when the menu is open prevents onClicked from
      // firing at all when a click closes the menu.  The menu closes itself
      // automatically when it loses focus.
      acceptedButtons: optionsMenu.opened ? Qt.NoButton : Qt.LeftButton
      focusCueColor: darkButtons ? Theme.popup.focusCueDarkColor : undefined
      onClicked: {
        // Fix up the menu position like other popups.
        var pos = Util.popupXYBindingFixup(optionsMenu,
                                           optionsMenu.parent.Window.window,
                                           optionsMenu.parent.Overlay.overlay,
                                           90, 55)
        optionsMenu.popup(headerBar, pos)
      }
    }
  }

  // Send the user back to the login page if we're logged out
  Connections {
    target: Daemon.account
    function onLoggedInChanged() {
      if (!Daemon.account.loggedIn) {
        pm.setPage('login')
      } else {
        pm.setPage('connect')
      }
    }
  }

  ThemedMenu {
    id: optionsMenu
    menuWidth: 200
    itemHeight: 30

    ThemedMenuItem {
      visible: ClientNotifications.updateAvailable.showInMenus
      enabled: ClientNotifications.updateAvailable.enableInMenus
      text: ClientNotifications.updateAvailable.menuText
      onTriggered: ClientNotifications.updateAvailable.menuSelected()
    }
    ThemedMenuSeparator {
      visible: ClientNotifications.updateAvailable.showInMenus
    }
    Action {
      text: uiTr("Settings")
      onTriggered: wSettings.showSettings()
    }
    Action {
      text: uiTr("Logout")
      enabled: Daemon.account.loggedIn
      onTriggered: Daemon.logout()
    }
    Action {
      text: uiTr("Quit")
      onTriggered: {
        console.info("Quit from header menu")
        Qt.quit()
      }
    }
  }
}
