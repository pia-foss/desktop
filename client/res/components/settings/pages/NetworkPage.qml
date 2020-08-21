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
import "."
import ".." // settings/ for SettingsMessages
import "../inputs"
import "../stores"
import "../../core"
import "../../client"
import "../../common"
import "../../daemon"
import "../../theme"
import PIA.NativeHelpers 1.0
import PIA.SplitTunnelManager 1.0
import PIA.BrandHelper 1.0

Page {
  ColumnLayout {
    anchors.fill: parent
    spacing: 10

    RowLayout {
      Layout.fillWidth: true
      spacing: 30

      DropdownInput {
        id: dnsDropdown
        //: Label for the setting that controls which DNS servers are used to look up
        //: domain names and translate them to IP addresses when browsing the internet.
        //: This setting is also present in OS network settings, so this string should
        //: preferably match whatever localized term the OS uses.
        label: uiTr("Name Servers")
        setting: Setting {
          id: customDNSSetting
          readonly property DaemonSetting daemonSetting: DaemonSetting { name: "overrideDNS"; onCurrentValueChanged: customDNSSetting.currentValue = currentValue }
          property var knownCustomServers: null
          sourceValue: daemonSetting.sourceValue

          function isStringValue(value, str) {
            return typeof value === 'string' && value === str
          }

          function isThirdPartyDNS(value) {
            return !isStringValue(value, "pia") && !isStringValue(value, "handshake") && !isStringValue(value, "local")
          }

          onSourceValueChanged: {
            if (Array.isArray(sourceValue) && sourceValue.length > 0) {
              knownCustomServers = sourceValue;
            }
          }
          onCurrentValueChanged: {
            if (currentValue === "custom") {
              customDNSDialog.setServers(Array.isArray(daemonSetting.currentValue) ? daemonSetting.currentValue : []);
            } else if (daemonSetting.currentValue !== currentValue) {
              // When changing from a non-third-party value (pia/handshake) to a
              // third-party value ("existing" since custom was already handled),
              // show the "use existing DNS" prompt.
              if (!isThirdPartyDNS(sourceValue) && isThirdPartyDNS(currentValue)) {
                customDNSDialog.setExisting();
              } else {
                daemonSetting.currentValue = currentValue;
              }
            }
          }
        }
        warning: {
          if(setting.isThirdPartyDNS(setting.sourceValue))
            return uiTr("Warning: Using a third party DNS could compromise your privacy.")
          return ""
        }
        info: {
          if(setting.sourceValue === "handshake") {
            //: "Handshake" is a brand name and should not be translated.
            return uiTr("Handshake is a decentralized naming protocol.  For more information, visit handshake.org.")
          }
          return ""
        }
        model: {
          var items = [
                { name: uiTr("PIA DNS"), value: "pia" },
                //: Indicates that we will run a built-in DNS resolver locally
                //: on the user's computer.
                { name: uiTr("Built-in Resolver"), value: "local" },
                // "Handshake" is a brand name, not translated.
                { name: "Handshake", value: "handshake" },
                { name: uiTr("Use Existing DNS"), value: "" },
                { name: uiTr("Set Custom DNS..."), value: "custom" },
              ];
          if (!NativeHelpers.includeFeatureHandshake)
            items.splice(2, 1)  // Delete Handshake

          if (Array.isArray(setting.currentValue) && setting.currentValue.length > 0) {
            items.splice(-1, 0, { name: setting.currentValue.join(" / "), value: setting.currentValue });
          } else if (setting.knownCustomServers) {
            items.splice(-1, 0, { name: setting.knownCustomServers.join(" / "), value: setting.knownCustomServers });
          }
          return items;
        }
        OverlayDialog {
          id: customDNSDialog
          buttons: [
            { text: uiTr("Proceed"), role: DialogButtonBox.AcceptRole },
            { text: uiTr("Cancel"), role: DialogButtonBox.RejectRole },
          ]
          canAccept: (!customPrimaryDNS.visible || (customPrimaryDNS.text.length > 0 && customPrimaryDNS.acceptableInput)) && (!customSecondaryDNS.visible || customSecondaryDNS.acceptableInput)
          contentWidth: 300
          RegExpValidator { id: ipValidator; regExp: /(?:(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])(?:\.(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])){3})?/ }
          ColumnLayout {
            width: parent.width
            TextboxInput {
              id: customPrimaryDNS
              label: uiTr("Primary DNS")
              setting: Setting { sourceValue: "" }
              validator: ipValidator
              Layout.fillWidth: true
            }
            TextboxInput {
              id: customSecondaryDNS
              label: uiTr("Secondary DNS (optional)")
              setting: Setting { sourceValue: "" }
              validator: ipValidator
              Layout.fillWidth: true
              Layout.bottomMargin: 5
            }
            DialogMessage {
              icon: 'warning'
              text: uiTr("<b>Warning:</b> Using non-PIA DNS servers could expose your DNS traffic to third parties and compromise your privacy.")
              color: Theme.settings.inputDescriptionColor
            }
          }
          function setServers(servers) {
            title = uiTr("Set Custom DNS");
            customPrimaryDNS.setting.currentValue = servers[0] || "";
            customSecondaryDNS.setting.currentValue = servers[1] || "";
            customPrimaryDNS.visible = true;
            customSecondaryDNS.visible = true;
            customPrimaryDNS.focus = true;
            open();
          }
          function setExisting() {
            title = customDNSSetting.currentValue ? uiTr("Use Custom DNS") : uiTr("Use Existing DNS");
            customPrimaryDNS.visible = false;
            customSecondaryDNS.visible = false;
            open();
          }
          onAccepted: {
            if (customPrimaryDNS.visible) {
              var servers = [];
              if (customPrimaryDNS.currentValue !== '')
                servers.push(customPrimaryDNS.currentValue);
              if (customSecondaryDNS.currentValue !== '')
                servers.push(customSecondaryDNS.currentValue);
              if (servers.length > 0) {
                customDNSSetting.daemonSetting.currentValue = servers;
              } else {
                customDNSSetting.currentValue = customDNSSetting.sourceValue;
              }
            } else {
              customDNSSetting.daemonSetting.currentValue = customDNSSetting.currentValue;
            }
            dnsDropdown.forceActiveFocus(Qt.MouseFocusReason)
          }
          onRejected: {
            customDNSSetting.currentValue = customDNSSetting.sourceValue;
            dnsDropdown.forceActiveFocus(Qt.MouseFocusReason)
          }
        }
      }
      ColumnLayout {
        spacing: 10

        CheckboxInput {
          id: portForwardCheckbox
          //: Label for the setting that controls whether the application tries to
          //: forward a port from the public VPN IP to the user's computer. This
          //: feature is not guaranteed to work or be available, therefore we label
          //: it as "requesting" port forwarding.
          label: uiTr("Request Port Forwarding")
          //: Tooltip for the port forwarding setting. The user can not choose which
          //: port to forward; a port will be automatically assigned by our servers.
          //: The user should further be made aware that only some of our servers
          //: support forwarding. The string contains embedded linebreaks to prevent
          //: it from being displayed too wide on the user's screen - such breaks
          //: should be preserved at roughly the same intervals.
          info: enabled ? uiTr("Forwards a port from the VPN IP to your computer. The port will be selected for you. Not all locations support port forwarding.") : ""
          warning: enabled ? "" : SettingsMessages.requiresOpenVpnMessage
          setting: DaemonSetting {
            override: !portForwardCheckbox.settingEnabled
            overrideValue: false
            name: "portForward"
          }
          // The legacy infrastructure only supports PF with OpenVPN.  The
          // modern infrastructure supports it with any protocol.
          // (reading "enabled" also considers parents' "enabled" flags, use an
          // intermediate property)
          readonly property bool settingEnabled: Daemon.settings.method === "openvpn" || Daemon.settings.infrastructure !== "current"
          enabled: settingEnabled
        }

        CheckboxInput {
          label: uiTr("Allow LAN Traffic")
          info: uiTr("Always permits traffic between devices on your local network, even when using the VPN killswitch.")
          setting: DaemonSetting { name: 'allowLAN' }
        }
      }
    }

    // This info tip has bullets and non-breaking spaces, it's broken up to
    // facilitate translation.
    readonly property string splitTunnelInfo: {
      var bullet = "\u2022\xA0\xA0"
      var endl = "\n"

      //: Description for the split tunnel setting.
      return uiTr("Choose which applications use the VPN.") + endl +
        //: Description for the "Bypass VPN" split tunnel mode that can be applied
        //: to a specific app.  These apps do not use the VPN connection, they
        //: connect directly to the Internet.
        bullet + uiTr("Bypass VPN - App always connects directly to the Internet") + endl +
        //: Description for the "Only VPN" split tunnel mode that can be applied
        //: to a specific app.  These apps are only allowed to connect via the
        //: VPN, they are blocked if the VPN is not connected (even if PIA is
        //: not running).
        bullet + uiTr("Only VPN - App can only connect when PIA is open and the VPN is connected")
    }

    CheckboxInput {
      id: appExclusionCheckboxMac
      visible: Qt.platform.os === 'osx'
      label: uiTr("Split Tunnel")
      info: parent.splitTunnelInfo
      linkText: Messages.helpLabel
      linkTarget: BrandHelper.getBrandParam("appExclusionsHelpLink")

      property bool inKextTest: false
      enabled: {
        // Always disable during a kext test
        if(inKextTest)
          return false
        // Disabling is always allowed if we're not in a test; this might be the
        // only way to clear a warning if the user gets into a state that does
        // not make sense.
        if(setting.currentValue)
          return true
        // Enable only if split tunnel support is available
        return Daemon.state.splitTunnelSupportErrors.length === 0
      }
      desc: {
        if(Daemon.state.netExtensionState === "NotInstalled")
          return uiTr("Approve the split tunnel extension to enable this feature.")

        // Unlike Windows, we can't show an "enabling will install" message - we
        // can't test if the kext is installed until either the setting is
        // checked or we try to connect with it enabled.

        return "";
      }
      descLinks: {
        if(Daemon.settings.method == "openvpn" && Daemon.state.netExtensionState === "NotInstalled") {
          return [{
            text: uiTr("Security Preferences"),
            clicked: function(){NativeHelpers.openSecurityPreferencesMac()}
          }]
        }
        return []
      }

      setting: Setting {
        id: appExclusionSettingMac

        readonly property DaemonSetting daemonSetting: DaemonSetting { name: "splitTunnelEnabled" }

        sourceValue: daemonSetting.sourceValue

        onCurrentValueChanged: {
          if(currentValue === daemonSetting.currentValue)
            return;

          // Disabling is always allowed
          if(!currentValue) {
            daemonSetting.currentValue = currentValue;
            return
          }

          // The checkbox is being enabled - check the install state
          if(Daemon.state.netExtensionState === "NotInstalled" ||
             Daemon.state.netExtensionState === "Unknown") {
            // We haven't checked the state yet, or it was not installed the
            // last time we checked.  Test the kext installation state
            // asynchronously, then apply or reject the setting change
            appExclusionCheckboxMac.inKextTest = true;
            Daemon.installKext(function (error, result) {
              appExclusionCheckboxMac.inKextTest = false;

              // What's the result of the kext test?  (result is the new
              // Daemon.state.netExtensionState that will be updated
              // asynchronously)
              if(!error && result === "Installed") {
                // It's installed, enable the setting
                daemonSetting.currentValue = true
              }
              else {
                // It's not installed, reject the setting change
                currentValue = daemonSetting.currentValue
              }
            });
            return
          }

          // Accept the setting change
          daemonSetting.currentValue = true
        }
      }
    }

    CheckboxInput {
      id: appExclusionCheckbox
      visible: Qt.platform.os !== 'osx'
      label: uiTr("Split Tunnel")
      info: parent.splitTunnelInfo
      linkText: Messages.helpLabel
      linkTarget: BrandHelper.getBrandParam("appExclusionsHelpLink")
      // On Windows, if the driver isn't installed and the setting isn't
      // enabled, show the "needs install" description.
      readonly property bool needsInstall: {
        return Qt.platform.os === 'windows' &&
          Daemon.state.netExtensionState === "NotInstalled" &&
          !setting.daemonSetting.currentValue
      }
      readonly property bool canStartInstall: {
        // Can't install if the OS is not supported.
        if(Daemon.state.splitTunnelSupportErrors.length)
          return false
        // We can start an install if no installation is going on right now, and
        // no reboot is required.
        return NativeHelpers.reinstallWfpCalloutStatus === "" ||
           NativeHelpers.reinstallWfpCalloutStatus === "success" ||
           NativeHelpers.reinstallWfpCalloutStatus === "error" ||
           NativeHelpers.reinstallWfpCalloutStatus === "denied"
      }
      enabled: {
        if(setting.currentValue) {
          // Disabling is always allowed - this might be the only way to clear
          // warnings if the driver has somehow been removed and can't be
          // installed.
          return true
        }
        if(canStartInstall) {
          // No installation occurring, and no reboot required
          return true
        }
        // If we're installing right now or a reboot is required, this setting
        // can't be enabled right now.  'desc' sets an appropriate message.
        return false
      }
      desc: {
        // The status of an ongoing installation overrides anything else.
        // (If the user manually installs the extension on Win 7 RTM, and they
        // get the "reboot" state, show that rather than the OS version error.)
        switch(NativeHelpers.reinstallWfpCalloutStatus) {
          default:
          case "":
          case "success":
          case "error":
          case "denied":
            break
          case "reboot":
            return SettingsMessages.installReboot
          case "working":
            // Only report the installation if it was started here.
            // (We still disable the control and/or report Reboot status, etc.,
            // even if the isntallation was started from the Help page.)
            if(setting.installingDriver)
              return uiTr("Installing split tunnel filter...")
        }

        var splitTunnelException = Daemon.state.splitTunnelSupportErrors
        if(splitTunnelException.length)
          return SettingsMessages.getSplitTunnelErrorDesc(splitTunnelException)

        if(Daemon.state.netExtensionState === "NotInstalled") {
          if(!setting.currentValue)
            return uiTr("Enabling this feature will install the split tunnel filter.")
          else {
            // This state normally shouldn't happen, but it is possible if the
            // driver was somehow uninstalled.
            // The daemon doesn't disable split tunnel if the driver is not
            // installed, because this state briefly occurs during a reinstall
            // too.
            return uiTr("The split tunnel filter is not installed.  Reinstall it on the Help page.")
          }
        }

        // Driver is installed or there was an error monitoring it.
        return ""
      }
      setting: Setting {
        id: appExclusionSetting
        readonly property DaemonSetting daemonSetting: DaemonSetting { name: 'splitTunnelEnabled' }
        sourceValue: daemonSetting.sourceValue

        // This is enabled whenever we trigger a driver install.  The help page
        // can also trigger a driver (re)install, so this indicates that we
        // should handle the result here.
        property bool installingDriver: false

        onCurrentValueChanged: {
          if(currentValue === daemonSetting.currentValue)
            return

          // Disabling is always allowed
          if(!currentValue) {
            daemonSetting.currentValue = currentValue
            return
          }

          // If we're already installing or a reboot is needed, we can't enable
          // (the control should be disabled in this case)
          if(!appExclusionCheckbox.canStartInstall) {
            currentValue = false  // Reject - should not have happened
            return
          }

          // Install the callout driver if needed when enabling this setting
          if(appExclusionCheckbox.needsInstall) {
            installingDriver = true
            NativeHelpers.installWfpCallout()
            // Don't apply the setting change yet, apply it when the install
            // completes.
          }
          else {
            // Otherwise, just apply the setting
            daemonSetting.currentValue = currentValue
          }
        }

        function checkInstallStatus() {
          if(!installingDriver)
            return
          switch(NativeHelpers.reinstallWfpCalloutStatus) {
            default:
            case "working":
              break
            case "success":
              installingDriver = false
              // Apply the setting change that triggered the install
              daemonSetting.currentValue = currentValue
              break
            case "reboot":
              installingDriver = false
              wSettings.alert(SettingsMessages.installReboot, uiTr("App Exclusions"), 'info')
              daemonSetting.currentValue = currentValue = false
              break
            case "error":
              installingDriver = false
              wSettings.alert(SettingsMessages.splitTunnelInstallError, uiTr("App Exclusions"), 'error')
              daemonSetting.currentValue = currentValue = false
              break
            case "denied":
              installingDriver = false
              daemonSetting.currentValue = currentValue = false
              break
          }
        }
      }

      Connections {
        target: NativeHelpers
        onReinstallWfpCalloutStatusChanged: appExclusionCheckbox.setting.checkInstallStatus()
      }
    }

    SplitTunnelSettings {
      Layout.fillHeight: true
      Layout.fillWidth: true
      Layout.topMargin: 1 // For check box's focus cue
      label: appExclusionCheckbox.label
      enabled: {
        // On any platform, disable if the split tunnel feature is not supported
        if(Daemon.state.splitTunnelSupportErrors.length)
          return false
        // On Windows, disable the split tunnel app list when the checkbox is
        // disabled, or if the driver hasn't been installed yet
        if(Qt.platform.os === 'windows')
          return appExclusionCheckbox.enabled && !appExclusionCheckbox.needsInstall && Daemon.settings.splitTunnelEnabled
        // On all other platforms, enable it all the time (on Linux, there's no
        // install; on Mac, we don't reliably know whether the kext has been
        // approved yet).
        return Daemon.settings.splitTunnelEnabled
      }
    }

    StaticText {
      Layout.fillWidth: true
      wrapMode: Text.WordWrap
      color: Theme.settings.inputDescriptionColor
      text: uiTr("Apps may need to be restarted for changes to be applied.")
    }
  }
}
