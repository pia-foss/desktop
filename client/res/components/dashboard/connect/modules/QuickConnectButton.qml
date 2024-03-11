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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../../daemon"
import "../../../theme"
import "../../../common"
import "../../../core"
import "../../../settings/stores"
import "../../../client"
import "../.."

Item {
  // Name of the location represented by this button, such as "us_chicago" or
  // "japan".  Should be a valid location from the server locations list, but
  // this can't be guaranteed since locations can disappear from the list.
  property string location
  readonly property bool hovered: connectMouseArea.containsMouse
  readonly property string locationName: Client.getFavoriteLocalizedName(location)

  width: 43
  height: 40

  Item {
    anchors.centerIn: parent
    width: 48
    height: 40

    Image {
      id: quickConnectingImage
      anchors.fill: parent

      property bool highlight: Daemon.state.connectingConfig.vpnLocation &&
                               Daemon.state.connectingConfig.vpnLocation.id === location
      source: Theme.dashboard.quickConnectingImage
      opacity: highlight ? 1.0 : 0.0

      HeaderOpacityBehavior on opacity {parent: quickConnectingImage}
    }

    Image {
      id: quickConnectedImage
      anchors.fill: parent

      // Only show the 'connected' highlight in the 'Connected' state; there
      // can be a connected location set in other states too.  In particular,
      // when disconnecting to reconnect to a new location, we only highlight
      // the new location.
      property bool highlight: Daemon.state.connectionState === 'Connected' &&
                               Daemon.state.connectedConfig.vpnLocation.id === location
      source: Theme.dashboard.quickConnectedImage
      opacity: highlight ? 1.0 : 0.0

      HeaderOpacityBehavior on opacity {parent: quickConnectedImage}
    }
  }

  FlagImage {
    id: flag
    anchors.centerIn: parent
    countryCode: Client.countryForFavorite(location)
    offline: Client.isFavoriteOffline(location)

    opacity: {
      // Stay 100% opaque when connected or connecting to this location.
      // Mouse clicks don't have any effect in this state, so we don't need to
      // highlight for points/clicks.
      if(quickConnectingImage.highlight || quickConnectedImage.highlight) {
         return 1.0
      }

      if(connectMouseArea.containsPress)
        return 1.0
      if(connectMouseArea.containsMouse)
        return 0.75
      return 0.5
    }
  }

  property ClientSetValueSetting favoriteSetting: ClientSetValueSetting {
    name: 'favoriteLocations'
    settingValue: location
  }

  readonly property bool isDedicatedIp: {
    var loc = Daemon.state.availableLocations[location]
    return !!(loc && loc.dedicatedIp)
  }

  // This is the silhouette of the DIP badge rendered in the background color of
  // the dashboard.  This prevents the flag from showing through the badge when
  // they're <100% opaque.
  //
  // We could alternately group these into a layer and alpha-blend the layer,
  // but this tweak works well enough to avoid the need to allocate layers for
  // these buttons.
  Image {
    anchors.horizontalCenter: flag.right
    anchors.verticalCenter: flag.top
    visible: isDedicatedIp
    source: Theme.dashboard.quickConnectDipBackgroundImage
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }

  // DIP badge itself.  Rendered with flag's opacity so it appears to be part
  // of the flag (changes with hover / select).
  Image {
    anchors.horizontalCenter: flag.right
    anchors.verticalCenter: flag.top
    visible: isDedicatedIp
    opacity: flag.opacity
    source: Theme.dashboard.quickConnectDipBadgeImage
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }

  Image {
    anchors.horizontalCenter: flag.right
    anchors.verticalCenter: flag.bottom
    visible: favoriteSetting.currentValue
    source: Theme.dashboard.quickConnectFavoriteImage
    width: sourceSize.width / 2
    height: sourceSize.height / 2
  }

  ButtonArea {
    id: connectMouseArea

    // Fill the full width - it's important that the adjacent buttons meet to
    // avoid juddery changes as the cursor scans over the buttons horizontally.
    // This makes for a large hit box, but that makes sense for something that's
    // intended to provide "quick access", and these have very good hover
    // feedback.
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: flag.top
    anchors.bottom: flag.bottom
    // Put the same margin around the top and bottom of the flag that we get on
    // the left and right
    readonly property real topBottomMargin: (width - flag.width) / 2
    anchors.topMargin: -topBottomMargin
    anchors.bottomMargin: -topBottomMargin

    name: {
      if(favoriteSetting.currentValue) {
        //: Screen reader annotation for a Quick Connect button with a heart
        //: icon, which is used for a favorite region.  %1 is a region name.
        return uiTr("Connect to favorite %1").arg(locationName)
      }
      //: Screen reader annotation for a Quick Connect button without a heart
      //: icon, which is for a recently-used or nearby location.  %1 is a region
      //: name.
      return uiTr("Connect to %1").arg(locationName)
    }

    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    // These are large hit boxes, put the focus cue on the inside, this also
    // keeps them from overlapping (which is only a little odd since only one
    // is shown at a time, but it still looks a tad off).
    focusCueInside: true
    focusCueForceRound: true

    onClicked: {
      if(!Client.isFavoriteOffline(location)) {
        Client.connectFavorite(location)
      }
    }
  }
}
