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

pragma Singleton
import QtQuick 2.0
import PIA.NativeClient 1.0
import PIA.NativeHelpers 1.0
import "../daemon"
import "../../javascript/util.js" as Util

QtObject {
  property ClientSettings settings: ClientSettings {}
  property ClientState state: ClientState {}
  property ClientUIState uiState: ClientUIState {}
  property ClientFeatures features: ClientFeatures {}

  // Valid recent locations (filters out any locations that no longer exist)
  readonly property var validRecentLocations: settings.recentLocations.filter(function(f) { return Daemon.state.availableLocations[f]; })

  // Display favorites in the following order:
  // 1. Dedicated IPs (sorted by region name)
  // 2. Country "best" favorites (sorted by country name)
  // 3. Normal regions (sorted by region name)
  //
  // Prior versions of PIA were (accidentally) comparing the "auto/<country>"
  // favorite code with display names for other regions, which mostly
  // sorted country groups first, except for countries like "Andorra", which
  // would sort before "auto".
  readonly property var favoriteSortGroups: ({
    dedicatedIps: 0,
    countryGroups: 1,
    regions: 2,
    invalid: 3
  })

  function determineFavoriteSortGroup(id, loc) {
    if(isValidAutoCountry(id))
      return favoriteSortGroups.countryGroups

    if(!loc)
      return favoriteSortGroups.invalid

    if(loc.dedicatedIpExpire > 0)
      return favoriteSortGroups.dedicatedIps

    return favoriteSortGroups.regions // Normal region
  }

  // Favorite locations sorted by alphabetically by display name
  // The user could control their order theoretically, but the interaction
  // for this would be cumbersome (re-favorite the regions in the desired
  // order), so it's better UX to use a fixed order in lieu of actually
  // giving a good re-order interaction, like drag-and-drop.
  readonly property var sortedFavorites: {
    // a location that starts with "auto/" is an "auto country" location, e.g "auto/us" - represents the server with lowest ping in the US.
    var favorites = (settings.favoriteLocations || []).filter(function(f) {  return Daemon.state.availableLocations[f] || isValidAutoCountry(f) })
    favorites.sort(function (first, second) {
      // Find location objects - these could be null if the region is a country
      // group
      var firstLoc = Daemon.state.availableLocations[first]
      var secondLoc = Daemon.state.availableLocations[second]
      // Determine each region's sort group
      var firstGroup = determineFavoriteSortGroup(first, firstLoc)
      var secondGroup = determineFavoriteSortGroup(second, secondLoc)

      if(firstGroup != secondGroup)
        return firstGroup - secondGroup

      // Names to compare initially - either region names or country names
      var firstName, secondName
      // Details to compare if the names are identical - currently only used for
      // DIPs, this is the IP address
      var firstDetail, secondDetail

      if(firstGroup === favoriteSortGroups.countryGroups) {
        firstName = Daemon.getCountryName(countryFromAutoCountryLocation(first))
        secondName = Daemon.getCountryName(countryFromAutoCountryLocation(second))
      }
      else {
        firstName = Daemon.getLocationName(firstLoc)
        secondName = Daemon.getLocationName(secondLoc)

        if(firstGroup === favoriteSortGroups.dedicatedIps) {
          firstDetail = firstLoc.dedicatedIp
          secondDetail = secondLoc.dedicatedIp
        }
      }

      var nameComp = firstName.localeCompare(secondName, state.activeLanguage.locale)
      if(nameComp !== 0)
        return nameComp
      return firstDetail.localeCompare(secondDetail, state.activeLanguage.locale)
    })
    return favorites
  }

  // an auto country is valid if it has >= 2 regions in it. If it only has 1 region then
  // we can just connect to that region directly and do not need to find the 'best' region in that country.
  function isValidAutoCountry(locationId) {
    return locationId.startsWith("auto/") && countryCount(countryFromAutoCountryLocation(locationId)) >= 2
  }

  // Return the number of regions for a given country
  function countryCount(country) {
    var countries = Daemon.state.groupedLocations
    for(var i=0; i<countries.length; ++i) {
      if(countries[i].locations.length <= 0)
        continue

      if(countries[i].locations[0].country.toUpperCase() === country.toUpperCase()) {
        return countries[i].locations.length
      }
    }

    return 0
  }

  function applySettings(settings) {
    return NativeClient.applySettings(settings)
  }

  function resetSettings() {
    NativeClient.resetSettings()
  }

  function localeUpperCase(text) {
    return NativeClient.localeUpperCase(text)
  }

  function countryFromLocation(location) {
    // connect by country
    if(location.startsWith("auto/")) {
      return countryFromAutoCountryLocation(location)
    }

    var locationData = Daemon.state.availableLocations[location]
    if(locationData && locationData.country)
      return locationData.country
    // Didn't find this location.
    return ""
  }

  // convert an auto/COUNTRY to a real location (finding the best region for that country)
  function realLocation(location) {
    if(location.startsWith("auto/")) {
      var country = countryFromAutoCountryLocation(location)
      return Daemon.state.getBestLocationForCountry(country)
    }
    else {
      return location
    }
  }

  // strip the "auto/" prefix from an Auto Country location
  // an auto country location represents the region with the lowest ping in a given country, e.g auto/us
  function countryFromAutoCountryLocation(location) {
    return location.substring(5, location.length).toUpperCase()
  }

  function getFavoriteLocalizedName(locId) {
    if(locId.startsWith("auto/")) {
      //: Text that indicates the best (lowest ping) region is being used for a given country.
      //: The %1 placeholder contains the name of the country, e.g "UNITED STATES - BEST"
      return uiTr("%1 - Best").arg(Daemon.getCountryName(countryFromAutoCountryLocation(locId)))
    }

    var loc = Daemon.state.availableLocations[locId]
    if(loc)
      return getDetailedLocationName(loc)

    // If all else fails, display the location ID - these should generally have
    // been filtered out though.
    return locId
  }

  // Get a "detailed" name for a location, for use in context like the tray menu
  // when additional details can't be represented in any other way than by
  // adding them to the name.  This includes enough detail to distinguish the
  // location by the name returned.
  //
  // Currently, this just adds the dedicated IP for dedicated IP locations.
  //
  // In most contexts, prefer to represent this information contextually when
  // possible (like in the regions list, etc.)
  function getDetailedLocationName(loc) {
    if(loc.dedicatedIp)
      return Daemon.getLocationName(loc) + " - " + loc.dedicatedIp

    return Daemon.getLocationName(loc)
  }

  // Start the log uploader.  If the daemon connection is up, asks the daemon to
  // write diagnostics.  (Even if this fails, the log uploader is still
  // started.)
  function startLogUploader() {
    // Log that we're trying to start an upload - there aren't any timeouts on
    // daemon RPCs, so if the daemon hangs (but the connection is not lost),
    // this will wait indefinitely.
    console.info("Requesting diagnostic dump to start log uploader")
    Daemon.writeDiagnostics(function(error, result) {
      if(error) {
        console.warn("Couldn't write diagnostics: " + error)
        // Start the tool anyway with no diagnostics file
        result = ''
      }

      NativeHelpers.startLogUploader(result)
    })
  }
}
