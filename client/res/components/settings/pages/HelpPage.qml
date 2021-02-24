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
import QtQuick.Window 2.11
import "."  // Local Page.qml instead of Page from QtQuick.Controls
import ".." // settings/ for SettingsMessages
import "../inputs"
import "../stores"
import "../../client"
import "../../daemon"
import "../../common"
import "../../theme"
import "qrc:/javascript/util.js" as Util
import PIA.NativeHelpers 1.0
import PIA.BrandHelper 1.0

Page {
  id: helpPage
  ColumnLayout {
    anchors.fill: parent
    anchors.leftMargin: Theme.settings.narrowPageLeftMargin
    // In Russian, the "Include Geo-Located Regions" translation just _barely_
    // runs over the right margin (<5 pixels), which causes it to wrap in the
    // horizontal layout.  This page is pretty full as it is, and the wrapping
    // looks bad since it's a just a few pixels over.
    //
    // Fudge the margin so it doesn't wrap.  Nothing else in any other language
    // is close to the margin, so this doesn't affect anything else.
    anchors.rightMargin: Math.max(0, Theme.settings.narrowPageLeftMargin - 20)
    spacing: 6

    // This text is treated as a static even though its text does label a
    // control, because it's displayed like a section heading, not a control
    // label.  (For comparison, the "Support" and "Maintenance" headings are
    // clearly not control labels, and they're displayed the same way as this
    // text.)
    StaticText {
      id: versionLabel
      text: uiTr("Version")
      color: Theme.settings.inputLabelColor
      font.pixelSize: Theme.settings.inputLabelTextPx
      font.bold: true
    }

    // If the client and daemon version are the same, just show that version
    // once.  If they're different, show both.
    readonly property bool differentVersions: Daemon.settings.lastUsedVersion !== NativeHelpers.getClientVersion()

    ValueHtml {
      label: versionLabel.text
      text: formatVersion(Daemon.settings.lastUsedVersion)
      visible: !parent.differentVersions
      color: Theme.settings.inputLabelColor
      font.pixelSize: Theme.settings.inputLabelTextPx
    }

    GridLayout {
      columnSpacing: 6
      rowSpacing: 6
      columns: 2
      visible: parent.differentVersions

      LabelText {
        id: clientLabel
        text: uiTr("Client:")
        font.pixelSize: Theme.settings.inputLabelTextPx
        color: Theme.settings.inputLabelColor
      }

      ValueHtml {
        label: clientLabel.text
        Layout.fillWidth: true
        text: formatVersion(NativeHelpers.getClientVersion())
        font.pixelSize: Theme.settings.inputLabelTextPx
        color: Theme.settings.inputLabelColor
      }

      LabelText {
        id: daemonLabel
        text: uiTr("Daemon:")
        font.pixelSize: Theme.settings.inputLabelTextPx
        color: Theme.settings.inputLabelColor
      }

      ValueHtml {
        label: daemonLabel.text
        Layout.fillWidth: true
        text: formatVersion(Daemon.settings.lastUsedVersion)
        font.pixelSize: Theme.settings.inputLabelTextPx
        color: Theme.settings.inputLabelColor
      }
    }

    TextLink {
        text: uiTr("Changelog")
        underlined: true
        onClicked: {
            wChangeLog.open();
        }
    }
    TextLink {
        //: This link displays the tour that users see initially after
        //: installation.
        text: uiTr("Quick Tour")
        underlined: true
        onClicked: {
            wOnboarding.showOnboarding();
        }
    }

    CheckboxInput {
      id: betaUpdatesCheckbox

      label: uiTr("Receive Beta Updates")
      info: uiTr("Join our beta program to test new features and provide feedback.")
      // Hide the beta checkbox if this brand doesn't have a beta channel.
      visible: BrandHelper.getBrandParam("brandReleaseChannelBeta") !== ""
      setting: Setting {
        id: betaUpdatesSetting
        readonly property DaemonSetting daemonSetting: DaemonSetting {
          name: "offerBetaUpdates"
          onCurrentValueChanged: betaUpdatesSetting.currentValue = currentValue
        }
        function applyCurrentValueChange() {
          if(currentValue === daemonSetting.currentValue)
            return

          // If beta is being enabled, show the agreement.  It'll apply the
          // change if it's accepted.
          if(currentValue) {
            betaAgreementDialog.open()
            betaAgreementDialog.setInitialFocus()
          }
          // Otherwise, just apply the change.
          else
            daemonSetting.currentValue = currentValue
        }
        Component.onCompleted: {
          betaUpdatesSetting.currentValue = daemonSetting.currentValue
          // Connect this signal after we've loaded the initial value; this is
          // mainly to avoid showing the prompt immediately if the daemon loads
          // with offerBetaUpdates=true
          betaUpdatesSetting.currentValueChanged.connect(applyCurrentValueChange)
        }
      }

      BetaAgreementDialog {
        id: betaAgreementDialog
        onAccepted: {
          Daemon.applySettings({offerBetaUpdates: true})
        }
        onRejected: {
          betaUpdatesSetting.currentValue = betaUpdatesSetting.sourceValue
          betaUpdatesCheckbox.forceActiveFocus(Qt.MouseFocusReason)
        }
      }
    }

    CheckboxInput {
      label: uiTr("Show service communication messages")
      setting: DaemonSetting { name: 'showAppMessages' }
    }

    // Spacer between groups
    Item {
      width: 1
      height: 4
    }

    StaticText {
      text: uiTr("Network")
      color: Theme.settings.inputLabelColor
      font.pixelSize: Theme.settings.inputLabelTextPx
      font.bold: true
    }

    CheckboxInput {
      label: uiTr("Include Geo-Located Regions")
      setting: DaemonSetting { name: 'includeGeoOnly' }
    }

    // Spacer between groups
    Item {
      width: 1
      height: 4
    }

    StaticText {
      text: uiTr("Support")
      color: Theme.settings.inputLabelColor
      font.pixelSize: Theme.settings.inputLabelTextPx
      font.bold: true
    }

    CheckboxInput {
      id: graphicsCheckbox
      label: uiTr("Disable Accelerated Graphics")
      visible: NativeHelpers.platform !== NativeHelpers.MacOS
      Connections {
        target:Client.settings
        function onDisableHardwareGraphicsChanged() {
          // A restart is required to change this setting.  If the setting is
          // toggled again though, it's back to the current state, so we don't
          // need to restart.
          if(Client.settings.disableHardwareGraphics !== Client.state.usingSafeGraphics) {
            wSettings.alert(uiTr("Restart Private Internet Access to apply this setting"),
                            graphicsCheckbox.label, 'info');
          }
        }
      }
      setting: ClientSetting {
        name: "disableHardwareGraphics"
      }
      info: uiTr("Accelerated graphics reduce CPU usage and enable graphical effects, but can cause issues with certain graphics cards or drivers.")
      infoShowBelow: false
    }

    CheckboxInput {
      label: uiTr("Enable Debug Logging")
      info: uiTr("Save debug logs which can be submitted to technical support to help troubleshoot problems.")
      infoShowBelow: false
      setting: Setting {
        sourceValue: Daemon.settings.debugLogging !== null
        onCurrentValueChanged: {
          if (currentValue !== sourceValue) {
            if (!currentValue) {
              Daemon.applySettings({ 'debugLogging': null });
            } else {
              Daemon.applySettings({ 'debugLogging': Daemon.settings.defaultDebugLogging});
            }
          }
        }
      }
    }

    TextLink {
      text: uiTr("Submit Debug Logs")
      underlined: true
      onClicked: Client.startLogUploader()
    }

    TextLink {
      text: uiTr("Support Portal")
      underlined: true
      link: BrandHelper.getBrandParam("helpDeskLink")
    }

    // Spacer between groups
    Item {
      width: 1
      height: 4
    }

    StaticText {
      text: uiTr("Maintenance")
      color: Theme.settings.inputLabelColor
      font.pixelSize: Theme.settings.inputLabelTextPx
      font.bold: true
    }

    ReinstallLink {
      id: reinstallTap
      visible: Qt.platform.os === 'windows'

      linkText: uiTr("Reinstall OpenVPN Network Adapter")
      executingText: uiTr("Reinstalling OpenVPN Network Adapter...")

      reinstallStatus: NativeHelpers.reinstallTapStatus
      reinstallAction: function(){NativeHelpers.reinstallTap()}
    }

    ReinstallLink {
      id: reinstallWinTun
      visible: Qt.platform.os === 'windows'

      linkText: uiTr("Reinstall WireGuard Network Adapter")
      executingText: uiTr("Reinstalling WireGuard Network Adapter...")

      reinstallStatus: NativeHelpers.reinstallTunStatus
      reinstallAction: function(){
        if(Daemon.state.wireguardAvailable)
          NativeHelpers.reinstallTun()
        else
          wSettings.alert(SettingsMessages.wgRequiresWindows8, "WireGuard", "info")
      }
    }

    TextLink {
      id: reinstallWfpCallout
      visible: Qt.platform.os === 'windows'

      // Like the check box on the Network page, this indicates whether an
      // ongoing installation was triggered by this link.
      property bool installingDriver: false

      text: {
        // Show the reinstalling message only when the install was really
        // started by this link
        if(installingDriver)
          return uiTr("Reinstalling Split Tunnel Filter...")
        return uiTr("Reinstall Split Tunnel Filter")
      }
      underlined: enabled
      enabled: {
        // installingDriver is set before the status updates (during the UAC
        // prompt), disable during this state.
        if(installingDriver)
          return false
        // Disable when any installation is ongoing, even if it was not started
        // by this link
        return NativeHelpers.reinstallWfpCalloutStatus === "" ||
          NativeHelpers.reinstallWfpCalloutStatus === "success" ||
          NativeHelpers.reinstallWfpCalloutStatus === "error" ||
          NativeHelpers.reinstallWfpCalloutStatus === "denied" ||
          NativeHelpers.reinstallWfpCalloutStatus === "reboot"
      }

      function startReinstall() {
        if(!enabled)
          return
        if(NativeHelpers.reinstallWfpCalloutStatus === "reboot") {
          wSettings.alert(SettingsMessages.installReboot, SettingsMessages.titleReinstallSuccessful, 'info')
          return
        }
        installingDriver = true
        NativeHelpers.reinstallWfpCallout()
      }

      onClicked: startReinstall()

      function checkInstallStatus() {
        if(!installingDriver)
          return
        switch(NativeHelpers.reinstallWfpCalloutStatus) {
          default:
          case "working":
            break
          case "success":
            installingDriver = false
            wSettings.alert(uiTr("The split tunnel filter was reinstalled."), SettingsMessages.titleReinstallSuccessful, 'info')
            break
          case "reboot":
            installingDriver = false
            wSettings.alert(SettingsMessages.installReboot, SettingsMessages.titleReinstallSuccessful, 'info')
            break
          case "error":
            installingDriver = false
            wSettings.alert(SettingsMessages.splitTunnelInstallError, SettingsMessages.titleReinstallError, 'error')
            break
          case "denied":
            installingDriver = false
            break
        }
      }

      Connections {
        target: NativeHelpers
        function onReinstallWfpCalloutStatusChanged() {
          reinstallWfpCallout.checkInstallStatus()
        }
      }
    }

    TextLink {
      text: uiTr("Uninstall Private Internet Access")
      underlined: true
      onClicked: NativeHelpers.quitAndUninstall()
    }

    // Vertical spacer
    Item {
      Layout.fillHeight: true
      width: 1
    }
  }

  Connections {
    target: ClientNotifications
    function onReinstallTapAdapter() {
      var settingsWindow = helpPage.Window.window
      settingsWindow.selectPage("help")
      settingsWindow.showSettings()
      reinstallTap.startReinstall()
    }
    function onReinstallWintun() {
      var settingsWindow = helpPage.Window.window
      settingsWindow.selectPage("help")
      settingsWindow.showSettings()
      reinstallWinTun.startReinstall()
    }
    function onReinstallSplitTunnel() {
      var settingsWindow = helpPage.Window.window
      settingsWindow.selectPage("help")
      settingsWindow.showSettings()
      reinstallWfpCallout.startReinstall()
    }
  }

  Connections {
    target: NativeHelpers
    function onTerminalStartFailed(cmd) {
      wSettings.open();
      //: "Terminal" refers to a terminal emulator in the Linux build, such
      //: as xterm, GNOME Terminal, Konsole, etc.  This should use the
      //: typical desktop terminology.
      var msg = uiTr("Failed to run command in terminal. Please install a terminal compatible with x-terminal-emulator.")
      msg += "<br><pre>" + cmd + "</pre>"
      wSettings.alert(msg, uiTr("Unable to open terminal"), 'error');
    }
  }

  // Format our standard version strings to be more human readable
  function formatVersion(versionString) {
    var v = versionString.split('+');
    var buildFields = (v.slice(1).join('+')).split('.');
    v = v[0].split('-');
    var text = uiTr("v%1").arg(v[0].replace(/\.0$/, ''));
    if (v.length > 1) {
      text += ' ' + v.slice(1).join(' ');
    }
    text += '<font color="' + Theme.settings.inputDescriptionColor + '">';
    if (buildFields.length === 1) {
      text += " (build " + buildFields[0] + ")";
    } else if (buildFields.length >= 3) {
      // Translation note - although this renders a date-time, it's part of the
      // build version, so it is rendered in a fixed format instead of a
      // localized format.
      // Note that this only appears for feature branch builds currently; even
      // if it appeared for releasable builds it should be rendered consistently
      // since it is part of the version label.
      buildFields[1] = buildFields[1].replace(/(....)(..)(..)(..)(..).*/, "$1-$2-$3 $4:$5")
      for (var i = 0 ; i < buildFields.length; i++) {
        text += " [" + buildFields[i] + "]";
      }
    }
    text += '</font>';
    return text;
  }
}
