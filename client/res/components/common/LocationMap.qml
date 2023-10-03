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
import "../client"
import "../core"
import "../daemon"
import "../theme"
import "qrc:/javascript/util.js" as Util

Item {
  id: worldMap

  // Radii of inner and outer part of location marker.
  property real markerInnerRadius
  property real markerOuterRadius
  // Opacity of map image
  property alias mapOpacity: mapImage.opacity
  // The location marked on the map (a location object from DaemonState)
  // If undefined, no location is marked.
  property var location

  Image {
    id: mapImage
    anchors.fill: parent
    source: Theme.dashboard.moduleRegionMapImage
  }

  Item {
    readonly property var locationGeo: {
      var geo
      // Get the region's metadata
      var regionMeta = Daemon.state.getRegionDisplay(location.id)
      if(regionMeta)
        geo = [regionMeta.geoLatitude, regionMeta.geoLongitude]
      // Verify that both coordinates are numbers - if we get any placeholder
      // like undefined / "None", ignore it.
      if(geo && Util.isFiniteNumber(geo[0]) && Util.isFiniteNumber(geo[1]))
        return geo
      return undefined;
    }

    // Top and bottom latitudes of map graphic
    readonly property real topLat: 83.65
    readonly property real bottomLat: -56.00
    // Top and bottom, Miller-projected
    readonly property real topMiller: millerProjectLat(topLat)
    readonly property real bottomMiller: millerProjectLat(bottomLat)
    // Left and right longitudes of map graphic
    readonly property real leftLong: -168.12
    readonly property real rightLong: -169.65

    function degToRad(degrees) {
      return degrees * Math.PI / 180.0
    }
    // Project latitude.  The map uses this projection:
    // https://en.wikipedia.org/wiki/Miller_cylindrical_projection
    function millerProjectLat(latitudeDeg) {
      return 1.25 * Math.log(Math.tan(Math.PI * 0.25 + 0.4 * degToRad(latitudeDeg)))
    }

    readonly property real locationX: {
      if(!locationGeo)
        return -1.0

      // Longitude is locationGeo[1] -> range [-180, 180]
      var x = locationGeo[1]
      // Adjust for actual left edge of graphic -> range [-168, 192]
      if(x < leftLong)
        x += 360.0
      // Map to [0, width]
      return (x - leftLong) / (rightLong - leftLong + 360.0) * worldMap.width
    }
    readonly property real locationY: {
      if(!locationGeo)
        return -1.0

      // Project the latitude -> range [-2.3034..., 2.3034...]
      var millerLat = millerProjectLat(locationGeo[0])
      // Map to the actual range shown by the map.  (If this point is outside
      // the map bound, it returns a negative value or a value greater than
      // height.)
      // Map to unit range -> [0, 1], where 0 is the bottom and 1 is the top
      var unitY = (millerLat - bottomMiller) / (topMiller - bottomMiller)
      // Flip and scale to height
      return (1.0-unitY) * worldMap.height
    }
    readonly property int mapPointX: {
      return Math.round(locationX)
    }
    readonly property int mapPointY: {
      return Math.round(locationY)
    }
    readonly property bool showLocation: {
      return mapPointX >= 0 && mapPointX < worldMap.width &&
             mapPointY >= 0 && mapPointY < worldMap.height
    }

    // We're aligning this dot to a non-directional image, so reflect X when RTL
    // mirror is on.
    x: {
      if(Client.state.activeLanguage.rtlMirror)
        return parent.width - mapPointX - markerOuterRadius
      else
        return mapPointX - markerOuterRadius
    }
    y: mapPointY - markerOuterRadius
    visible: showLocation
    width: markerOuterRadius * 2
    height: markerOuterRadius * 2

    // Outer location marker
    Rectangle {
      anchors.fill: parent
      radius: markerOuterRadius
      color: Theme.dashboard.locationMarkerOuterColor
    }

    // Inner location marker
    Rectangle {
      anchors.centerIn: parent
      radius: markerInnerRadius
      width: markerInnerRadius * 2
      height: markerInnerRadius * 2
      color: Theme.dashboard.locationMarkerCenterColor
    }
  }
}
