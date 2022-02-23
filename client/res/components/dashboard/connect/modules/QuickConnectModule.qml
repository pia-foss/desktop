// Copyright (c) 2022 Private Internet Access, Inc.
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
import "../../../theme"
import "../../../client"
import "../../../core"
import "../../../daemon"
import "../../../../javascript/util.js" as Util
import PIA.NativeAcc 1.0 as NativeAcc

MovableModule {
  implicitHeight: 80
  moduleKey: 'quickconnect'

  //: Screen reader annotation for the Quick Connect tile.
  tileName: uiTr("Quick Connect tile")
  NativeAcc.Group.name: tileName

  Text {
    text: {
      var hoveredLocation = quickConnectButtons.hoveredLocationName
      if(hoveredLocation)
        return Client.localeUpperCase(hoveredLocation)

      return uiTr("QUICK CONNECT")
    }

    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 10
    width: 260
    elide: Text.ElideRight
  }

  RowLayout {
    id: quickConnectButtons

    x: 19
    y: 30
    height: 40
    spacing: 0

    property string hoveredLocationName: {
      for(var i=0; i<children.length; ++i) {
        if(children[i].hovered)
          return children[i].locationName
      }

      return ""
    }

    Repeater {
      id: buttonRepeater

      // Add recent locations to buttonLocs up to the button limit.  These are
      // kept in order of most-recently-used.
      function addRecentLocations(buttonLocs, maxButtons) {
        for(var recentIdx = 0;
            buttonLocs.length < maxButtons &&
            recentIdx < Client.validRecentLocations.length;
            ++recentIdx) {
          // If this recent isn't already in the list and isn't 'auto', add it
          var recent = Client.validRecentLocations[recentIdx]
          if(recent !== 'auto' && !Util.arrayIncludes(buttonLocs, recent))
            buttonLocs.push(recent)
        }
      }

      // Add nearest locations without adding any locations from a country
      // that's already present (from a favorite, recent, or another nearest
      // location).
      function addNearestLocations(buttonLocs, maxButtons) {
        // If we have 6 buttons, we're done, don't bother sorting locations or
        // computing used countries
        if(buttonLocs.length >= maxButtons)
          return

        // If we still don't have 6 buttons, grab locations sorted by latency
        // with a max of one per country.  (Only one per country, because North
        // American users would mostly see a bunch of US flags otherwise, which
        // looks strange as a first impression.)

        // Track the countries that have already been used by favorites/recents
        var usedCountries = {}
        buttonLocs.forEach(function (id) {
          var loc = Daemon.state.availableLocations[id]
          if(loc)
            usedCountries[loc.country] = true
        })

        // Grab the first location from each country that hasn't already been
        // used.  (No need to add these to usedCountries because we won't
        // revisit a country from this list.)
        for(var groupIdx = 0;
            buttonLocs.length < maxButtons && groupIdx < Daemon.state.groupedLocations.length;
            ++groupIdx) {
          var firstCountryLoc = Daemon.state.groupedLocations[groupIdx].locations[0]
          if(!usedCountries[firstCountryLoc.country])
            buttonLocs.push(firstCountryLoc.id)
        }
      }

      model: {
        // Maximum of 6 buttons
        var maxButtons = 6

        // Grab explicit favorites first.
        var buttonLocs = Client.sortedFavorites.slice(0, maxButtons)

        // If the user doesn't have 6 favorites selected, use some recent
        // locations to fill up the list.
        addRecentLocations(buttonLocs, maxButtons)

        // If we *still* don't have 6 buttons, add nearest locations.
        addNearestLocations(buttonLocs, maxButtons)

        return buttonLocs
      }

      QuickConnectButton {
        id: button
        location: modelData
      }
    }
  }
}
