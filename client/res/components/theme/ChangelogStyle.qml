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

import QtQuick 2.10
QtObject {

  readonly property color tabBackgroundColor: dark ? "#121322" : "#E4E4E4"
  readonly property color tabTextColor: dark ? "#FFFFFF" : "212B36"
  readonly property color tabHighlightColor: "#39B54B"


  readonly property color whatsNewBackgroundGradientStart: dark ? "#103035" : "#f0f0f0"
  readonly property color whatsNewBackgroundGradientEnd: dark ? "#121521" : "#f0f0f0"

  readonly property string wireguardBgImage: Theme.imagePath + "/changelog/wireguard-bg.png"
  readonly property string wireguardCheckImage: Theme.imagePath + "/changelog/checkmark.png"
  readonly property string ctaButtonDisabledImage: Theme.imagePath + "/changelog/wireguard-cta-disabled.png"
  readonly property string ctaButtonEnabledImage: Theme.imagePathCommon + "/changelog/wireguard-cta-button.png"
  readonly property color learnMoreLinkColor: dark ? "#A6B0BE" : "#637381"


  readonly property string wireguardPartAsteroid: Theme.imagePathCommon + "/changelog/wireguard-asteroid.png"
  readonly property string wireguardPartMoon: Theme.imagePathCommon + "/changelog/wireguard-moon.png"
  readonly property string wireguardPartRocket: Theme.imagePathCommon + "/changelog/wireguard-rocket.png"
  readonly property string wireguardPartCircle: Theme.imagePathCommon + "/changelog/wireguard-circle.png"

  readonly property color separatorBorderColor: "#979797"

  readonly property string splitTunnelImage: Theme.imagePath + "/changelog/split-tunnel.png"
  readonly property string terminalImage: Theme.imagePath + "/changelog/terminal.png"
  readonly property string dedicatedIpImage: Theme.imagePathCommon + "/changelog/dedicated-ip-globe.png"
  readonly property string automationImage: Theme.imagePathCommon + "/changelog/automation.png"
  readonly property string wireguardImage: Theme.imagePathCommon + "/changelog/wireguard.png"
  readonly property color heroCtaTextColor: Theme.login.buttonEnabledTextColor
  readonly property color heroCtaDisabledTextColor: dark ? "#ffffff" : "#323642"
}
