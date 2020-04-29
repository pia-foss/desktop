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

  // Favorite locations sorted by alphabetically by display name
  // The user could control their order theoretically, but the interaction
  // for this would be cumbersome (re-favorite the regions in the desired
  // order), so it's better UX to use a fixed order in lieu of actually
  // giving a good re-order interaction, like drag-and-drop.
  readonly property var sortedFavorites: {
    // a location that starts with "auto/" is an "auto country" location, e.g "auto/us" - represents the server with lowest ping in the US.
    var favorites = (settings.favoriteLocations || []).filter(function(f) {  return Daemon.state.availableLocations[f] || isValidAutoCountry(f) })
    favorites.sort(function (first, second) {
      return Util.compareNoCase(Daemon.getLocationIdName(first),
                                Daemon.getLocationIdName(second))
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

      if(countries[i].locations[0].country.toUpperCase() == country.toUpperCase()) {
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

  // Find the best location for a country - taking into account port forwarding
  // and also whether a country is 'safe' to connect to
  function bestLocationForCountry(country) {
    var countries = Daemon.state.groupedLocations
    for(var i=0; i<countries.length; ++i) {
      if(countries[i].locations.length <= 0)
        continue

      if(countries[i].locations[0].country == country) {
        var countryLocations = countries[i].locations
        var result = null

        if(Daemon.settings.portForward) {
          // try most specific filter first (portForward support + safe, i.e respects auto_regions)
          result = countryLocations.filter(function(loc) { return loc.portForward && loc.autoSafe })[0]
          if (result) { return result.id }
        }

        // if cannot find entries, loosen filter slightly (we can't find a region that supports port forwarding, so skip it)
        result = countryLocations.filter(function(loc) { return loc.autoSafe })[0]
        if(result) { return result.id }

        // failing that, fall back to just the region in this country with the lowest ping
        return countryLocations[0].id
      }
    }
  }

  // convert an auto/COUNTRY to a real location (finding the best region for that country)
  function realLocation(location) {
    if(location.startsWith("auto/")) {
      var country = countryFromAutoCountryLocation(location)
      return bestLocationForCountry(country)
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

  function getFavoriteLocalizedName(location) {
    if(location.startsWith("auto/"))
      //: Text that indicates the best (lowest ping) region is being used for a given country.
      //: The %1 placeholder contains the name of the country, e.g "UNITED STATES - BEST"
      return uiTr("%1 - Best").arg(Daemon.getCountryName(countryFromAutoCountryLocation(location)))
    else
      return Daemon.getLocationIdName(location)
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
