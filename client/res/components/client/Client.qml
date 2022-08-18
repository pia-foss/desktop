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

pragma Singleton
import QtQuick 2.0
import PIA.NativeClient 1.0
import PIA.NativeHelpers 1.0
import "../daemon"
import "../vpnconnection"
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

    if(loc.dedicatedIp)
      return favoriteSortGroups.dedicatedIps

    return favoriteSortGroups.regions // Normal region
  }

  // Get the localized name of a country by country code.
  function getCountryName(countryCode) {
    // Depend on uiTr and Daemon.state.regionsMetadata so this will reevaluate
    // if they change; the native implementation can't add that dependency
    let trDep = uiTr
    let metaDep = Daemon.state.regionsMetadata
    return NativeClient.state.getTranslatedCountryName(countryCode)
  }
  // Get the localized prefix for a region in a country by country code.
  function getCountryPrefix(countryCode) {
    let trDep = uiTr
    let metaDep = Daemon.state.regionsMetadata
    return NativeClient.state.getTranslatedCountryPrefix(countryCode)
  }
  // Get a region's country name - this is suitable for displaying a region
  // that's alone in a country; it's not nested under a country group.
  function getRegionCountryName(regionId) {
    let countryCode = Daemon.state.getRegionCountryCode(regionId)
    return getCountryName(countryCode)
  }
  // Get a region's city name.  This is typically not used by itself in PIA,
  // we combine it with the country prefix - see getRegionNestedName().
  function getRegionCityName(regionId) {
    let trDep = uiTr
    let metaDep = Daemon.state.regionsMetadata
    return NativeClient.state.getTranslatedRegionName(regionId)
  }
  // Get a name for a region displayed nested under a country group.  This
  // combines the country's prefix with the region's name.
  function getRegionNestedName(regionId) {
    let countryCode = Daemon.state.getRegionCountryCode(regionId)
    return getCountryPrefix(countryCode) + getRegionCityName(regionId)
  }
  // Select either the country name or nested name for a region, depending on
  // whether there are any other regions in this country.
  //
  // In some cases the caller may have already decided whether to create a
  // single entry or a group, but it may still be desirable to call this if
  // the regions might have been filtered.  Filtering might cause a nested
  // region to become the only "filtered" region in its country, and this
  // shouldn't change the region's apparent name.
  function getRegionAutoName(regionId) {
    console.assert(typeof regionId === 'string', "Expected a string as regionId, got " + typeof regionId)
    let countryCode = Daemon.state.getRegionCountryCode(regionId)
    if(!Daemon.state.shouldNestCountry(countryCode))
      return getCountryName(countryCode)  // Single-region country
    // Otherwise it's a multiple-region country, build the nested name
    return getCountryPrefix(countryCode) + getRegionCityName(regionId)
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
      var firstDetail = "", secondDetail = ""

      if(firstGroup === favoriteSortGroups.countryGroups) {
        firstName = getCountryName(countryFromAutoCountryLocation(first))
        secondName = getCountryName(countryFromAutoCountryLocation(second))
      }
      else {
        firstName = getRegionAutoName(firstLoc.id)
        secondName = getRegionAutoName(secondLoc.id)

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
    return locationId.startsWith("auto/") &&
      Daemon.state.shouldNestCountry(countryFromAutoCountryLocation(locationId))
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

  ///////////////
  // Favorites //
  ///////////////
  // Favorite regions can be:
  // - an ordinary region (favorite ID is just the region ID)
  // - a country group (favorite ID is "auto/<country>", such as "auto/DE" or
  //   "auto/US")
  //
  // Connecting to a "country group" favorite just connects to the current
  // "best" region for that country.  We cannot currently actually select a
  // "country group" as the current location, so it will not update if the best
  // region in that country changes.
  //
  // These utilities provide operations that work on either kind of favorite,
  // using the favorite name.

  // These operations on country groups are generally only used here to build
  // the general operations
  function isCountryGroupFavorite(location) {
    return location.startsWith("auto/")
  }
  function countryFromAutoCountryLocation(location) {
    return location.substring(5, location.length).toUpperCase()
  }

  // Get the country code for a favorite; used to show flags.
  function countryForFavorite(location) {
    // connect by country
    if(isCountryGroupFavorite(location)) {
      return countryFromAutoCountryLocation(location)
    }

    let countryCode = Daemon.state.getRegionCountryCode(location)
    return countryCode || ""
  }

  function connectFavorite(location) {
    if(isCountryGroupFavorite(location)) {
      VpnConnection.connectCountryBest(countryFromAutoCountryLocation(location))
    }
    else {
      VpnConnection.connectLocation(location)
    }
  }

  // Check if a favorite is offline
  function isFavoriteOffline(location) {
    if(isCountryGroupFavorite(location)) {
      let country = countryFromAutoCountryLocation(location)
      // A country group is online if at least one region in the country is
      // online.  Although the daemon applies some preferences to try to select
      // auto-safe or non-geo regions first, these only affect preference order.
      let countryRegions = Daemon.state.getLocationsInCountry(country)
      return !countryRegions.find(region => !region.offline)
    }

    // Otherwise, just check the offline flag for this location
    let l = Daemon.state.availableLocations[location]
    return !l || l.offline;
  }

  function getFavoriteLocalizedName(locId) {
    if(isCountryGroupFavorite(locId)) {
      //: Text that indicates the best (lowest ping) region is being used for a given country.
      //: The %1 placeholder contains the name of the country, e.g "UNITED STATES - BEST"
      return uiTr("%1 - Best").arg(getCountryName(countryFromAutoCountryLocation(locId)))
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
      return getRegionAutoName(loc.id) + " - " + loc.dedicatedIp

    return getRegionAutoName(loc.id)
  }

  // Start the log uploader.  If the daemon connection is up, asks the daemon to
  // write diagnostics.  (Even if this fails, the log uploader is still
  // started.)
  function startLogUploader() {
    if(uiState.settings.gatheringDiagnostics)
      return;
    // Log that we're trying to start an upload - there aren't any timeouts on
    // daemon RPCs, so if the daemon hangs (but the connection is not lost),
    // this will wait indefinitely.
    console.info("Requesting diagnostic dump to start log uploader")
    uiState.settings.gatheringDiagnostics = true;

    Daemon.writeDiagnostics(function(error, result) {
      uiState.settings.gatheringDiagnostics = false;
      if(error) {
        console.warn("Couldn't write diagnostics: " + error)
        // Start the tool anyway with no diagnostics file
        result = ''
      }

      NativeHelpers.startLogUploader(result)
    })
  }
}
