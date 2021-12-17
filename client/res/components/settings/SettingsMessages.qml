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

pragma Singleton
import QtQuick 2.0
import PIA.SplitTunnelManager 1.0

// These are Settings-related messages used in multiple places (so they're only
// translated once).  Messages that are used in just one place can be left
// in-source.
QtObject {
  // Title for successful reinstallation messages
  readonly property string titleReinstallSuccessful: uiTranslate("HelpPage", "Reinstall successful")
  // Title for failed reinstallation messages
  readonly property string titleReinstallError: uiTranslate("HelpPage", "Reinstall error")
  // Message for success with reboot.  Used for a message box on the help page
  // and description text on the network page.
  readonly property string installReboot: uiTr("Restart your computer to finish installing the split tunnel filter.")
  // Split tunnel filter couldn't be installed
  readonly property string splitTunnelInstallError: uiTr("The split tunnel filter could not be installed.  Try restarting your computer, or contact support if this problem persists.")
  // Messages for platforms where split tunnel isn't supported
  readonly property var splitTunnelSupportErrors: {
    "win_version_invalid": uiTr("This feature requires Windows 7 Service Pack 1 or later."),
    "iptables_invalid": uiTr("This feature requires iptables 1.6.1 or later."),
    //: Message for Linux indicating that specific system libraries are needed
    //: to support the split tunnel feature.
    "libnl_invalid": uiTr("This feature requires libnl-3, libnl-route-3, and libnl-genl-3."),
    //: Message for Linux indicating that a kernel feature has to be mounted at
    //: a specific location to use the split tunnel feature.  %1 is a file path,
    //: currently /sys/fs/cgroups/net_cls.
    "cgroups_invalid": uiTr("This feature requires net_cls to be available at %1.").arg(SplitTunnelManager.linuxNetClsPath),
    "cn_proc_invalid": uiTr("This feature requires kernel process events.")
  }
  function getSplitTunnelErrorDesc(errors) {
    if(errors.length === 0)
      return ""
    var desc = SettingsMessages.splitTunnelSupportErrors[errors[0]]
    for(var i=1; i<errors.length; ++i) {
      desc += '\n'
      desc += SettingsMessages.splitTunnelSupportErrors[errors[i]]
    }
    return desc
  }

  readonly property string requiresOpenVpnMessage: uiTr("This feature requires OpenVPN.")
  readonly property string wgRequiresWindows8: uiTr("WireGuard requires Windows 8 or later.")

  // Labels for connection settings - used on Connection page and in Connection
  // tile
  readonly property string connectionTypeSetting: uiTranslate("ConnectionPage", "Transport")
  readonly property string remotePortSetting: uiTranslate("ConnectionPage", "Remote Port")
  readonly property string dataEncryptionSetting: uiTranslate("ConnectionPage", "Data Encryption")
  readonly property string dataAuthenticationSetting: uiTranslate("ConnectionPage", "Data Authentication")
  readonly property string handshakeSetting: uiTranslate("ConnectionPage", "Handshake")
  readonly property string defaultRemotePort: uiTranslate("ConnectionPage", "Default")

  readonly property string mtuSetting: uiTr("MTU")
  readonly property string mtuSettingAuto: uiTranslate("mtu_setting", "Auto")
  readonly property string mtuSettingLargePackets: uiTranslate("mtu_setting", "Large Packets")
  readonly property string mtuSettingSmallPackets: uiTranslate("mtu_setting", "Small Packets")
  readonly property string mtuSettingDescription: [
      uiTr("Determines the maximum packet size allowed through the tunnel."),
      uiTr("Auto: Detect automatically, best for most connections"),
      uiTr("Large Packets: Most efficient if the connection is reliable"),
      uiTr("Small Packets: Less efficient but best on unreliable connections")
    ].join("\n\u2022\xA0\xA0")
  
  readonly property string stMontereyConfirmation: uiTr("You may encounter internet connection issues on macOS Monterey when the Split Tunnel feature is turned on. Please disable the Split Tunnel feature if you're having trouble connecting to the internet.")
  readonly property string stMontereyNotification: uiTr("Please disable the Split Tunnel feature if you're having trouble connecting to the internet.")
  readonly property string stMontereyPushNotification: uiTr("macOS Monterey has introduced an issue with Split Tunnel on some systems.  If you can't reach the internet, disable Split Tunnel.")
  readonly property string stMontereyPushLink: uiTr("Disable Split Tunnel")
}
