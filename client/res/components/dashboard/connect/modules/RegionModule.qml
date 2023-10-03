// Copyright (c) 2023 Private Internet Access, Inc.
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
import "../../../client"
import "../../../theme"
import "../../../common"
import "../../../core"

MovableModule {
  id: regionModule
  implicitHeight: {
    if(proxyVia.visible)
      return proxyVia.y + 40
    if(dipSubtitle.visible)
      return dipSubtitle.y + 36 // This text is a little smaller
    return regionValue.y + 40
  }
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
  // Lock-in the displayed location when already connected.
  // Otherwise a region list update might result in the incorrect location being shown.
  // This could happen if the 'chosen' location (which was offline
  // when we connected) was to come back online.
  readonly property var chosenLocation: {
    if(Daemon.state.connectionState === "Connected")
    {
        return !Daemon.state.connectedConfig.vpnLocationAuto ?
            Daemon.state.connectedConfig.vpnLocation : null
    }

    return Daemon.state.vpnLocations.chosenLocation
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
        return Daemon.state.vpnLocations.chosenLocation.geoLocated
      else if(regionModule.autoLocation)
        return regionModule.autoLocation.geoLocated
      return false
    }
  }

  Text {
    id: regionValue
    text: Messages.displayLocationSelection(regionModule.chosenLocation,
                                            regionModule.autoLocation)

    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: geoTip.visible ? (geoTip.x + geoTip.width + 6) : 20
    y: 40
    width: rightChevron.x - 5 - x
    elide: Text.ElideRight
  }

  DedicatedIpSubtitle {
    id: dipSubtitle
    x: 20
    y: 60
    visible: {
      if(regionModule.chosenLocation)
        return !!regionModule.chosenLocation.dedicatedIp
      return false
    }
    dedicatedIp: {
      if(regionModule.chosenLocation)
        return regionModule.chosenLocation.dedicatedIp
      return ""
    }
    // Fill the DIP tag with the background color if it's shown, since it may
    // otherwise allow part of the map to show though.  This ignores the
    // background change when the module is moved, but that's fine, the result
    // is that this looks like a label stuck on the module, which it is.
    dipTagBackground: Theme.dashboard.backgroundColor
  }

  function displayConnectionProxy(config) {
    // We don't need any complex logic for displaying the proxy setting
    // described by ConnectionConfig; any interacting settings are handled by
    // ConnectionConfig in the daemon (it'll force the effective proxy to 'none'
    // when using WireGuard, etc.)
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
    //
    // TODO - This duplicates logic from ConnectionConfig.  It'd be better for
    // the daemon to express a ConnectionConfig describing the "next"
    // connection state too, so the UI just renders a ConnectionConfig in all
    // cases.
    if(Daemon.settings.method !== "openvpn" || !Daemon.settings.proxyEnabled)
      return {type: 'none', dest: ''}

    switch(Daemon.settings.proxyType) {
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
        return {type: 'none', dest: ''}
    }
  }

  StaticText {
    id: proxyLabelText
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
    y: (dipSubtitle.visible ? dipSubtitle.y : regionValue.y) + 30
    visible: proxyVia.visible
  }

  StaticText {
    id: proxyVia
    text: proxyLabels.dest

    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: 20
    y: proxyLabelText.y + 20
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
    description: {
      // regionValue handles "auto" choices, but if we're displaying a chosen
      // location, we want the detailed description to cover the possibility of
      // a dedicated IP region being chosen.
      var descRegion = regionModule.chosenLocation ?
        Client.getDetailedLocationName(chosenLocation) : regionValue.text
      //: Screen reader description for the Region tile button.  Should begin with
      //: the "Select region" translation, since that's what the button does.
      //: This also includes the currently-selected region, which the button
      //: displays.  %1 is a region name.
      uiTr("Select region, %1 is selected").arg(descRegion)
    }

    cursorShape: Qt.PointingHandCursor
    // Show this focus cue on the inside, since it fills the full module
    focusCueInside: true
    onClicked: {
      pageManager.setPage(pageManager.pageIndices.region)
    }
  }
}
