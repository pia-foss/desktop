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

import './tabs'
import "./pages"

SecondaryWindow {
  id: settings

  contentLogicalWidth: tabLayout.item.implicitWidth
  contentLogicalHeight: tabLayout.item.implicitHeight

  // Symbolic constants for the pages in 'pages', to be used with selectPage()
  property var page: {
    'general': 0,
    'account': 1,
    'privacy': 2,
    'network': 3,
    'connection': 4,
    'help': 5,
  }

  // Limit InfoTips' texts to this width (both because the settings window is
  // pretty wide, and so it's consistent for both horizontal and vertical
  // layouts)
  readonly property int infoTipMaxWidth: 500

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
    'account': QT_TR_NOOP("Account", 'setting-title'),
    'privacy': QT_TR_NOOP("Privacy", 'setting-title'),
    'network': QT_TR_NOOP("Network", 'setting-title'),
    'connection': QT_TR_NOOP("Connection", 'setting-title'),
    'proxy': QT_TR_NOOP("Proxy", 'setting-title'),
    'help': QT_TR_NOOP("Help", 'setting-title'),
  }
  readonly property var pageHeadings: {
    'general': QT_TR_NOOP("General Preferences", 'setting-heading'),
    'account': QT_TR_NOOP("Account Information", 'setting-heading'),
    'privacy': QT_TR_NOOP("Privacy Preferences", 'setting-heading'),
    'network': QT_TR_NOOP("Network Preferences", 'setting-heading'),
    'connection': QT_TR_NOOP("Connection Preferences", 'setting-heading'),
    'proxy': QT_TR_NOOP("Proxy Preferences", 'setting-heading'),
    'help': QT_TR_NOOP("Help", 'setting-heading'),
  }
  property var pages: [
    {
      name: 'general',
      component: "GeneralPage.qml"
    },
    {
      name: 'account',
      component: "AccountPage.qml"
    },
    {
      name: 'privacy',
      component: "PrivacyPage.qml"
    },
    {
      name: 'network',
      component: "NetworkPage.qml"
    },
    {
      name: 'connection',
      component: "ConnectionPage.qml"
    },
    {
      name: 'proxy',
      component: "ProxyPage.qml"
    },
    {
      name: 'help',
      component: "HelpPage.qml"
    }
  ]

  // As usual, the call to uiTr() must occur in the same context as
  // QT_TR_NOOP() in order to translate correctly, these functions are passed
  // into the tab layouts to translate strings.
  function getPageTitle(pageId) {
    return uiTr(pageTitles[pageId], 'setting-title')
  }
  function getHeadingTitle(pageId) {
    return uiTr(pageHeadings[pageId], 'setting-heading')
  }

  function selectPage(index) {
    tabLayout.item.selectPage(index)
  }

  property var currentAlert
  property var pendingAlerts: []
  // alert(message [, title = "Alert" [, icon = 'warning']] [, buttons] [, cb])
  // 'icon' must be one of the icon identifiers defined in DialogMessage (info,
  // warning, or error)
  function alert() {
    var args = Array.prototype.slice.call(arguments);
    var obj = {};
    if (args.length && typeof args[args.length - 1] === 'function') {
      obj.cb = args.pop();
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

  Component {
    id: hbarComponent
    HorizontalTabLayout {
      pages: settings.pages
      pageTitleFunc: getPageTitle
    }
  }
  Component {
    id: vbarComponent
    VerticalTabLayout {
      pages: settings.pages
      pageTitleFunc: getPageTitle
      pageHeadingFunc: getHeadingTitle
    }
  }

  Loader {
    id: tabLayout
    sourceComponent: Theme.settings.horizontal ? hbarComponent : vbarComponent
    width: parent.width
    height: parent.height
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
    function onShowPrivacyPage(){
      selectPage(page.privacy)
      showSettings()
    }
    function onShowNetworkPage() {
      selectPage(page.network)
      showSettings()
    }
    function onShowConnectionPage() {
      selectPage(page.connection)
      showSettings()
    }
    function onShowHelpPage() {
      selectPage(page.help)
      showSettings()
    }
    function onShowHelpAlert() {
      selectPage(page.help)
      showSettings()
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
