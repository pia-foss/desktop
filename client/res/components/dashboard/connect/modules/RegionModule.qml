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
import "../../../../javascript/app.js" as App
import "../../../daemon"
import "../../../theme"
import "../../../common"
import "../../../core"

MovableModule {
  id: regionModule
  implicitHeight: proxyVia.visible ? 130 : 80
  moduleKey: 'region'

  // RegionModule doesn't need a group since the whole region is one button, but
  // a name is still set because ModuleLoader uses it in the "bookmark" button's
  // name.
  //: Screen reader annotation for the Region tile, which users can click to
  //: go to the Region page and choose a region.
  tileName: uiTr("Region tile")

  // When connected with 'auto', show the connected location name (it might
  // not be the current best location if the best location has changed).
  // Otherwise, show the best location name.
  readonly property var autoLocation: {
    if(Daemon.state.connectionState === "Connected")
      return Daemon.state.connectedConfig.vpnLocation
    return Daemon.state.vpnLocations.bestLocation
  }

  LocationMap {
    width: 120
    height: 60
    x: 140
    y: 10
    markerInnerRadius: 2
    markerOuterRadius: 4
    mapOpacity: Theme.dashboard.moduleRegionMapOpacity
    location: {
      // When connected, show the connected location
      if(Daemon.state.connectionState === "Connected")
        return Daemon.state.connectedConfig.vpnLocation

      // Otherwise, show the location we would connect to next
      return Daemon.state.vpnLocations.nextLocation
    }
  }

  Text {
    text: uiTr("VPN SERVER")
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 20
  }

  GeoTip {
    id: geoTip
    x: 20
    anchors.verticalCenter: regionValue.verticalCenter
    visible: {
      if(Daemon.state.vpnLocations.chosenLocation)
        return Daemon.state.vpnLocations.chosenLocation.geoOnly
      else if(regionModule.autoLocation)
        return regionModule.autoLocation.geoOnly
      return false
    }
  }

  Text {
    id: regionValue
    text: {
      return Messages.displayLocationSelection(Daemon.state.vpnLocations.chosenLocation,
                                               regionModule.autoLocation)
    }

    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: geoTip.visible ? (geoTip.x + geoTip.width + 6) : 20
    y: 40
    width: rightChevron.x - 5 - x
    elide: Text.ElideRight
  }

  function displayConnectionProxy(config) {
    // Proxy is only supported for OpenVPN currently
    if(config.method !== "openvpn")
      return {type: 'none', dest: ''}

    switch(config.proxy) {
      case 'custom':
        return {type: 'custom', dest: config.proxyCustom}
      case 'shadowsocks':
        var region = Messages.displayLocation(config.proxyShadowsocks,
                                              config.proxyShadowsocksLocationAuto)
        return {type: 'shadowsocks', dest: region}
      default:
      case 'none':
        return {type: 'none', dest: ''}
    }
  }
  readonly property var proxyLabels: {
    // When connected, display the connected proxy
    if(Daemon.state.connectionState === "Connected")
      return displayConnectionProxy(Daemon.state.connectedConfig)
    // When connecting, display the connecting proxy
    if(Daemon.state.connectingConfig.vpnLocation)
      return displayConnectionProxy(Daemon.state.connectingConfig)

    // Otherwise, display the configured proxy
    // Only supported for OpenVPN currently
    if(Daemon.settings.method !== "openvpn")
      return {type: 'none', dest: ''}

    switch(Daemon.settings.proxy) {
      case 'custom':
        var host = Daemon.settings.proxyCustom.host
        if(Daemon.settings.proxyCustom.port) {
          host += ':'
          host += Daemon.settings.proxyCustom.port
        }
        if(host)
          return {type: 'custom', dest: host}
        return {type: 'none', dest: ''}
      case 'shadowsocks':
        var region = Messages.displayLocationSelection(Daemon.state.shadowsocksLocations.chosenLocation,
                                                       Daemon.state.shadowsocksLocations.bestLocation)
        return {type: 'shadowsocks', dest: region}
      default:
      case 'none':
        return {type: 'none', dest: ''}
    }
  }

  StaticText {
    text: {
      switch(proxyLabels.type) {
        case 'custom':
          return uiTr("VIA SOCKS5 PROXY")
        case 'shadowsocks':
          return uiTr("VIA SHADOWSOCKS")
        default:
        case 'none':
          return ""
      }
    }
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 70
    visible: proxyVia.visible
  }

  StaticText {
    id: proxyVia
    text: proxyLabels.dest

    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: 20
    y: 90
    visible: !!text
  }

  Image {
    id: rightChevron
    anchors.right: parent.right
    anchors.rightMargin: 15
    y: 30
    rtlMirror: true
    height: 20
    width: 10
    source: Theme.dashboard.moduleRightChevronImage
  }

  ButtonArea {
    anchors.fill: parent
    // The border covers the top of the module, put the mouse area at the bottom
    // of the border
    anchors.topMargin: Theme.dashboard.moduleBorderPx

    //: Screen reader annotation for the button that makes up the Region tile,
    //: which users can click to go to the Region page and select a region.
    //: Should be a short description of the "select region" action.
    name: uiTr("Select region")
    //: Screen reader description for the Region tile button.  Should begin with
    //: the "Select region" translation, since that's what the button does.
    //: This also includes the currently-selected region, which the button
    //: displays.  %1 is a region name.
    description: uiTr("Select region, %1 is selected").arg(regionValue.text)

    cursorShape: Qt.PointingHandCursor
    // Show this focus cue on the inside, since it fills the full module
    focusCueInside: true
    onClicked: {
      pageManager.setPage("region")
    }
  }
}
