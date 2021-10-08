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
import QtGraphicalEffects 1.0
import "../common"
import "../core"
import "../daemon"
import "../theme"
import "../settings"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util

Rectangle {
  id: newChangelog
  color: Theme.changelog.whatsNewBackgroundGradientEnd

  implicitHeight: callingCardContainer.implicitHeight

  //: Screen reader annotation for the "checkmark" bullets used in
  //: the What's New view
  readonly property string checkBulletLabel: uiTranslate("ChangelogWindow", "Checkmark bullet")

  Column {
    id: callingCardContainer
    anchors.fill: parent
    HeroCard {
      width: parent.width
    }

    CallingCard {
      width: parent.width
      headlineText: uiTr("WireGuard Protocol")
      bodyText: uiTr("It's a new, more efficient protocol that offers better performance, lower CPU usage, and longer battery life.  [[Try it now]]")
      onMessageLinkClicked: {
        wSettings.showPage('protocol')
        // The Protocol setting uses a PrivacyInput, so it will show a nice
        // sliding animation if the protocol changes due to this RPC, which
        // makes it obvious that we applied a change.
        Daemon.applySettings({"method":"wireguard"})
      }
      sourceImage: Theme.changelog.wireguardImage
      imgDisplayLeftBleed: 43
      imgDisplayRightBleed: 37
      imgDisplayTopBleed: 8
      imgDisplayBottomBleed: 31
      //: Screen reader annotation for the WireGuard image in the
      //: What's New view
      imageAnnotation: uiTr("WireGuard feature")
    }

    CallingCard {
      width: parent.width
      headlineText: uiTranslate("AutomationPage", "Connection Automation")
      bodyText: uiTr("Create rules to automatically connect or disconnect the VPN when you join a particular network.  [[Go to Settings]]")
      onMessageLinkClicked: wSettings.showPage('automation')
      sourceImage: Theme.changelog.automationImage
      //: Screen reader annotation for the automation image in the
      //: What's New view
      imageAnnotation: uiTr("Automation feature")
    }

    CallingCard {
      width: parent.width
      headlineText: uiTranslate("ChangelogWindow",
                                "Control which apps use our VPN!")

      //: The [[double square brackets]] are replaced by a link. Please use them for the relavant link text.
      bodyText: uiTranslate(
                  "ChangelogWindow",
                  "With our comprehensive split tunneling functionality, you can control which apps use the VPN and when. Check out our Split Tunnel [[app examples]] to see what's possible.")
      onMessageLinkClicked: Qt.openUrlExternally(
                    "https://www.privateinternetaccess.com/helpdesk/kb/articles/split-tunnel-app-examples")
      sourceImage: Theme.changelog.splitTunnelImage
      //: Screen reader annotation for the split tunnel image in the
      //: What's New view
      imageAnnotation: uiTranslate("ChangelogWindow", "Split tunnel feature")
    }

    CallingCard {
      width: parent.width
      headlineText: uiTranslate("ChangelogWindow", "Have you tried our command-line interface (CLI)?")

      bodyText: uiTranslate(
                  "ChangelogWindow",
                  "The PIA desktop app comes with a CLI for added functionality. Invoke \"piactl\" and check it out today! You can do things like control PIA, integrate PIA hotkeys, and even automate PIA functionality with scripts, or make custom integrations.")
      sourceImage: Theme.changelog.terminalImage
      //: Screen reader annotation for the command-line interface image in the
      //: What's New view
      imageAnnotation: uiTranslate("ChangelogWindow", "Command-line interface feature")
    }
  }
}
