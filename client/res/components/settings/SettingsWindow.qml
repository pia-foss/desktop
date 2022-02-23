// Copyright (c) 2022 Private Internet Access, Inc.
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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.10
import PIA.FocusCue 1.0
import PIA.WindowScaler 1.0
import PIA.WindowMaxSize 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import PIA.NativeHelpers 1.0
import "qrc:/javascript/util.js" as Util
import "../theme"
import "../common"
import "../core"
import "../client"
import "../daemon"

import './tabs'
import "./pages"

SecondaryWindow {
  id: settings

  // The Settings window's size is determined by its content (it can depend on
  // the size of translations), but the content is unloaded when the window
  // isn't shown.
  //
  // When the content is loaded, use the content's size.  This happens in the
  // right order when the window is shown, the initial "dummy" size isn't
  // displayed.
  //
  // When the content is unloaded, remember the last size it reported.  This
  // _is_ important to preserve the window size as it's being hidden; if we
  // reverted to a dummy size when the content was unloaded, the dummy size
  // would be visible during the "close" animation from the window manager.
  contentLogicalWidth: 200
  contentLogicalHeight: 200

  // The "dedicated IP" page currently is shown based on a feature toggle, or
  // if any dedicated IPs have already been added.
  readonly property bool showDedicatedIp: Daemon.data.flags.includes("dedicated_ip") ||
    Daemon.state.dedicatedIpLocations.length > 0

  // Limit InfoTips' texts to this width (both because the settings window is
  // pretty wide, and so it's consistent for both horizontal and vertical
  // layouts)
  readonly property int infoTipMaxWidth: 500

  // Open the Settings window to a particular page
  function showPage(page) {
    selectPage(page)
    showSettings()
  }

  // Although the 'pages' model is an array and could use uiTr(), it's also used
  // as the model for a Repeater containing the settings pages, meaning all the
  // settings pages would be destroyed and recreated when it changes.
  //
  // This would cause issues with the Language drop-down (specifically, opening
  // the popup and clicking a language would cause a crash, although
  // arrow-keying through languages does not).
  //
  // To avoid that, we still use the ListModel-style workaround of breaking the
  // translated strings out of the model.
  readonly property var pageTitles: {
    'general': QT_TR_NOOP("General", 'setting-title'),
    'protocol': QT_TR_NOOP("Protocols", 'setting-title'),
    'network': QT_TR_NOOP("Network", 'setting-title'),
    'privacy': QT_TR_NOOP("Privacy", 'setting-title'),
    'dedicated-ip': QT_TR_NOOP("Dedicated IP", 'setting-title'),
    'automation': QT_TR_NOOP("Automation", 'setting-title'),
    'split-tunnel': QT_TR_NOOP("Split Tunnel", 'setting-title'),
    'multihop': QT_TR_NOOP("Multi-Hop", 'setting-title'),
    'account': QT_TR_NOOP("Account", 'setting-title'),
    'help': QT_TR_NOOP("Help", 'setting-title'),
  }

  property var pages: {
    var p = [
      {
        name: 'general',
        component: "general/GeneralPage.qml"
      },
      {
        name: 'protocol',
        component: "protocol/ProtocolPage.qml"
      },
      {
        name: 'network',
        component: "network/NetworkPage.qml"
      },
      {
        name: 'privacy',
        component: "privacy/PrivacyPage.qml"
      },
      {
        name: 'dedicated-ip',
        component: "dedicated-ip/DedicatedIpPage.qml"
      },
      {
        name: 'automation',
        component: "automation/AutomationPage.qml"
      },
      {
        name: 'split-tunnel',
        component: "split-tunnel/SplitTunnelPage.qml"
      },
      {
        name: 'multihop',
        component: "multihop/MultihopPage.qml"
      },
      {
        name: 'account',
        component: "account/AccountPage.qml"
      },
      {
        name: 'help',
        component: "help/HelpPage.qml"
      },
    ]

    return p
  }

  // As usual, the call to uiTr() must occur in the same context as
  // QT_TR_NOOP() in order to translate correctly, these functions are passed
  // into the tab layouts to translate strings.
  function getPageTitle(pageId) {
    return uiTr(pageTitles[pageId], 'setting-title')
  }
  function getHeadingTitle(pageId) {
    return uiTr(pageTitles[pageId], 'setting-title')
  }

  function selectPage(id) {
    var index = pages.findIndex(page => page.name === id)
    if(index >= 0)
      Client.uiState.settings.currentPage = index
  }

  property var currentAlert
  property var pendingAlerts: []
  // alert(message [, title = "Alert" [, icon = 'warning']] [, buttons] [, cb])
  // 'icon' must be one of the icon identifiers defined in DialogMessage (info,
  // warning, or error)
  function alert() {
    var args = Array.prototype.slice.call(arguments);
    var obj = {};
    if (args.length && typeof args[args.length - 1] === 'function') { obj.cb = args.pop();
    }
    if (args.length && Array.isArray(args[args.length - 1])) {
      obj.buttons = args.pop();
    } else {
      obj.buttons = [Dialog.Ok];
    }
    obj.message = '' + args[0];
    obj.title = args[1];
    if (typeof obj.title !== 'string') obj.title = uiTr("Alert");
    obj.icon = args[2];
    if (typeof obj.icon !== 'string') obj.icon = 'warning';
    if (!currentAlert) {
      currentAlert = obj;
      setupAndShowAlert();
    } else {
      pendingAlerts.push(obj);
    }
  }
  function setupAndShowAlert() {
    alertDialogLoader.alertText = currentAlert.message;
    alertDialogLoader.alertTitle = currentAlert.title;
    alertDialogLoader.alertIcon = currentAlert.icon;
    alertDialogLoader.alertButtons = currentAlert.buttons;
    alertDialogLoader.active = true;
    alertDialogLoader.item.open();
  }
  Connections {
    target: alertDialogLoader.item
    onClicked: {
      if (settings.currentAlert.cb) {
        settings.currentAlert.cb(button);
      }
    }
    onClosed: {
      alertDialogLoader.active = false;
      settings.currentAlert = settings.pendingAlerts.shift();
      if (settings.currentAlert) {
        settings.setupAndShowAlert();
      }
    }
  }

  title: uiTr("Settings")

  color: Theme.settings.backgroundColor

  // Show the settings window.  Used by the header menu and tray icon menu.
  function showSettings() {
    open()
  }

  function updateWindowSize() {
    if(tabLayout.item) {
      settings.contentLogicalWidth = tabLayout.item.implicitWidth
      settings.contentLogicalHeight = tabLayout.item.implicitHeight
    }
  }
  Component {
    id: vbarComponent
    VerticalTabLayout {
      pages: settings.pages
      pageTitleFunc: getPageTitle
      pageHeadingFunc: getHeadingTitle
      onImplicitWidthChanged: settings.updateWindowSize()
      onImplicitHeightChanged: settings.updateWindowSize()
    }
  }

  Loader {
    id: tabLayout
    sourceComponent: vbarComponent
    width: parent.width
    height: parent.height
    active: settings.visible || settings.positioningForShow
    onItemChanged: settings.updateWindowSize()
  }

  Component {
    id: alertDialogComponent
    OverlayDialog {
      title: alertTitle
      buttons: alertButtons
      ColumnLayout {
        width: parent.width
        DialogMessage {
          id: alertMessage
          text: alertText
          icon: alertIcon
          Layout.minimumWidth: 200
          Layout.maximumWidth: 400
        }
      }
    }
  }
  Loader {
    id: alertDialogLoader
    active: false
    sourceComponent: alertDialogComponent
    property string alertText
    property string alertTitle
    property string alertIcon
    property var alertButtons
  }

  ReconnectWarning {
    anchors.left: parent.left
    anchors.bottom: parent.bottom
    anchors.margins: 20
  }

  Connections {
    target: ClientNotifications
    function onShowPage(pageName) {
      settings.showPage(pageName)
    }
    function onShowPrivacyPage(){
      settings.showPage('privacy')
    }
    function onShowNetworkPage() {
      settings.showPage('network')
    }
    function onShowConnectionPage() {
      settings.showPage('protocol')
    }
    function onShowMultihopPage() {
      settings.showPage('multihop')
    }
    function onShowHelpPage() {
      settings.showPage('help')
    }

    function startDriverReinstall(driverName) {
      settings.showPage('help')
      let helpPage = tabLayout.item && tabLayout.item.getCurrentPage()
      if(helpPage)
        helpPage.startDriverReinstall(driverName)
    }

    function onReinstallTapAdapter() {
      startDriverReinstall('tap')
    }
    function onReinstallWintun() {
      startDriverReinstall('wintun')
    }
    function onReinstallSplitTunnel() {
      startDriverReinstall('callout')
    }
    function onShowHelpAlert(msg, title, level) {
      settings.showPage('help')
      alert(msg, title, level)
    }
  }

  // Cmd+comma is the typical "preferences" shortcut on Mac OS.
  Shortcut {
    sequence: StandardKey.Preferences
    context: Qt.ApplicationShortcut
    onActivated: showSettings()
  }

  function showWrapDemo(width, text) {
    wrapContainer.wrapWidth = width
    wrapContainer.demo = text
  }
}
