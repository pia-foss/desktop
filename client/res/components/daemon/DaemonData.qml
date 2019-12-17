// Copyright (c) 2019 London Trust Media Incorporated
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

import QtQuick 2.0
import PIA.NativeDaemon 1.0

QtObject {
  readonly property var locations: NativeDaemon.data.locations
  readonly property var certificateAuthorities: NativeDaemon.data.certificateAuthorities

  // The release versions/URIs shouldn't be used by the client, these are just
  // the persistent data for UpdateDownloader.  They're only provided so they
  // show up in dev tools.
  readonly property string gaChannelVersion: NativeDaemon.data.gaChannelVersion
  readonly property string gaChannelVersionUri: NativeDaemon.data.gaChannelVersionUri
  readonly property string betaChannelVersion: NativeDaemon.data.betaChannelVersion
  readonly property string betaChannelVersionUri: NativeDaemon.data.betaChannelVersionUri

  readonly property var udpPorts: NativeDaemon.data.udpPorts
  readonly property var tcpPorts: NativeDaemon.data.tcpPorts

  // Location metadata - info we need about the locations, but that isn't
  // provided by the server list.
  // Currently contains:
  // - Latitude/longitude
  // - Translatable name
  //
  // Note that the names here are just marked with QT_TR_NOOP, they are not
  // actually translated in the metadata.  Daemon.getLocationName() checks if
  // the name from the server list still matches the name that was translated,
  // and only uses the translation if it does.  (This handles cases like
  // "Germany" becoming "DE Frankfurt", which happened when "DE Berlin" was
  // added - without this check, we would have displayed
  // "DE" -> "Germany", "DE Berlin" when the regions were updated.)
  //
  // Also, note that Qt's doc calls the function "qsTrNoOp", but the name is
  // actually QT_TR_NOOP per the code sample and qqmlbuiltinfunctions.cpp (1791)
  readonly property var locationMetadata: {
    "ae": {lat: 23.424076, long: 53.847818, name: QT_TR_NOOP("UAE")},
    "aus": {lat: -33.868820, long: 151.209296, name: QT_TR_NOOP("AU Sydney")},
    "aus_melbourne": {lat: -37.813628, long: 144.963058, name: QT_TR_NOOP("AU Melbourne")},
    "aus_perth": {lat: -31.950527, long: 115.860458, name: QT_TR_NOOP("AU Perth")},
    "austria": {lat: 47.516231, long: 14.550072, name: QT_TR_NOOP("Austria")},
    "belgium": {lat: 50.503887, long: 4.469936, name: QT_TR_NOOP("Belgium")},
    "brazil": {lat: -14.235004, long: -51.92528, name: QT_TR_NOOP("Brazil")},
    "ca": {lat: 45.501689, long: -73.567256, name: QT_TR_NOOP("CA Montreal")},
    "ca_toronto": {lat: 43.653226, long: -79.383184, name: QT_TR_NOOP("CA Toronto")},
    "ca_vancouver": {lat: 49.282729, long: -123.120738, name: QT_TR_NOOP("CA Vancouver")},
    "czech": {lat: 50.075538, long: 14.4378, name: QT_TR_NOOP("Czech Republic")},
    "de_berlin": {lat: 52.520007, long: 13.404954, name: QT_TR_NOOP("DE Berlin")},
    "denmark": {lat: 56.263920, long: 9.501785, name: QT_TR_NOOP("Denmark")},
    "fi": {lat: 61.924110, long: 25.748151, name: QT_TR_NOOP("Finland")},
    "france": {lat: 46.227638, long: 2.213749, name: QT_TR_NOOP("France")},
    "germany": {lat: 50.110922, long: 8.682127, name: QT_TR_NOOP("DE Frankfurt")}, //Frankfurt
    "hk": {lat: 22.396428, long: 114.109497, name: QT_TR_NOOP("Hong Kong")},
    "hungary": {lat: 47.162494, long: 19.503304, name: QT_TR_NOOP("Hungary")},
    "in": {lat: 20.593684, long: 78.96288, name: QT_TR_NOOP("India")},
    "ireland": {lat: 53.142367, long: -7.692054, name: QT_TR_NOOP("Ireland")},
    "israel": {lat: 31.046051, long: 34.851612, name: QT_TR_NOOP("Israel")},
    "italy": {lat: 41.871940, long: 12.56738, name: QT_TR_NOOP("Italy")},
    "japan": {lat: 36.204824, long: 138.252924, name: QT_TR_NOOP("Japan")},
    "lu": {lat: 49.815273, long: 6.129583, name: QT_TR_NOOP("Luxembourg")},
    "mexico": {lat: 23.634501, long: -102.552784, name: QT_TR_NOOP("Mexico")},
    "nl": {lat: 52.132633, long: 5.291266, name: QT_TR_NOOP("Netherlands")},
    "no": {lat: 60.472024, long: 8.468946, name: QT_TR_NOOP("Norway")},
    "nz": {lat: -40.900557, long: 174.885971, name: QT_TR_NOOP("New Zealand")},
    "poland": {lat: 51.919438, long: 19.145136, name: QT_TR_NOOP("Poland")},
    "ro": {lat: 45.943161, long: 24.96676, name: QT_TR_NOOP("Romania")},
    "sg": {lat: 1.352083, long: 103.819836, name: QT_TR_NOOP("Singapore")},
    "spain": {lat: 40.463667, long: -3.74922, name: QT_TR_NOOP("Spain")},
    "sweden": {lat: 60.128161, long: 18.643501, name: QT_TR_NOOP("Sweden")},
    "swiss": {lat: 46.818188, long: 8.227512, name: QT_TR_NOOP("Switzerland")},
    "turkey": {lat: 38.963745, long: 35.243322, name: QT_TR_NOOP("Turkey")},
    "uk": {lat: 51.507351, long: -0.127758, name: QT_TR_NOOP("UK London")}, // London
    "uk_manchester": {lat: 53.480759, long: -2.242631, name: QT_TR_NOOP("UK Manchester")},
    "uk_southampton": {lat: 50.909700, long: -1.404351, name: QT_TR_NOOP("UK Southampton")},
    "us2": {lat: 36.414652, long: -77.739258, name: QT_TR_NOOP("US East")}, // East
    "us3": {lat: 40.607697, long: -120.805664, name: QT_TR_NOOP("US West")},
    "us_atlanta": {lat: 33.748995, long: -84.387982, name: QT_TR_NOOP("US Atlanta")},
    "us_california": {lat: 36.778261, long: -119.417932, name: QT_TR_NOOP("US California")},
    "us_chicago": {lat: 41.878114, long: -87.629798, name: QT_TR_NOOP("US Chicago")},
    "us_denver": {lat: 39.739236, long: -104.990251, name: QT_TR_NOOP("US Denver")},
    "us_florida": {lat: 27.664827, long: -81.515754, name: QT_TR_NOOP("US Florida")},
    "us_houston": {lat: 29.760427, long: -95.369803, name: QT_TR_NOOP("US Houston")},
    "us_las_vegas": {lat: 36.169941, long: -115.13983, name: QT_TR_NOOP("US Las Vegas")},
    "us_new_york_city": {lat: 40.712775, long: -74.005973, name: QT_TR_NOOP("US New York City")},
    "us_seattle": {lat: 47.606209, long: -122.332071, name: QT_TR_NOOP("US Seattle")},
    "us_silicon_valley": {lat: 37.593392, long: -122.04383, name: QT_TR_NOOP("US Silicon Valley")},
    "us_south_west": {lat: 33.623962, long: -109.654814, name: QT_TR_NOOP("US Texas")},
    "us_washington_dc": {lat: 38.907192, long: -77.036871, name: QT_TR_NOOP("US Washington DC")},
    "za": {lat: -30.559482, long: 22.937506, name: QT_TR_NOOP("South Africa")}
  }

  // The regions list doesn't give us country names either.  We only need them
  // for countries with more than one region.
  readonly property var countryNames: {
    "de": QT_TR_NOOP("Germany"),
    "ca": QT_TR_NOOP("Canada"),
    "us": QT_TR_NOOP("United States"),
    "au": QT_TR_NOOP("Australia"),
    "gb": QT_TR_NOOP("United Kingdom")
  }

  // Translate a location name.  Used by Daemon.getLocationName() and
  // Daemon.getCountryName() - the call to uiTr() has to occur in the same file
  // as the QT_TR_NOOP() so the context is correct.
  function translateName(name) {
    return uiTr(name)
  }

  // Check location metadata and log diagnostics for devs - such as new
  // locations that should be added, old locations that might be removable,
  // name changes, etc.
  function checkLocationMetadata(groupedLocations) {
    var loc, meta
    // Log if any locations in the server list are missing metadata that have to
    // be manually added.
    for(var locId in locations) {
      loc = locations[locId]
      if(!locationMetadata[locId]) {
        console.info('Location ' + loc.name + ' (' + locId + ') does not have metadata')
      }
      else {
        // Check translatable names against the names the server reports.  This
        // detects server names that have changed and need to be re-localized.
        meta = locationMetadata[locId]
        if(loc.name !== meta.name) {
          console.info('Location ' + locId + ' has changed name: ' + meta.name +
                       ' -> ' + loc.name)
        }
      }
    }

    // Log any locations that might have been removed - metadata that doesn't
    // correspond to an actual location.
    // Do this only if at least one location exists.  (In case the locations
    // haven't been loaded at all by the daemon, don't log a bunch of spurious
    // warnings.  This is mainly for dev diagnostics so it doesn't have to be
    // perfect.)
    if(locations.length > 0) {
      for(var metaId in locationMetadata) {
        if(!locations[metaId]) {
          meta = locationMetadata[metaId]
          console.info('Location ' + meta.name + ' (' + metaId + ') has metadata but was not listed by server')
        }
      }
    }

    // Check for countries that have more than one location but are missing a
    // name.
    for(var i=0; i<groupedLocations.length; ++i) {
      if(groupedLocations[i].locations.length > 1) {
        var countryCode = groupedLocations[i].locations[0].country
        if(!countryNames[countryCode.toLowerCase()])
          console.info('Country name missing for ' + countryCode)
      }
    }
  }
}
