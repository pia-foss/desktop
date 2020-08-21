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

import QtQuick 2.0
import PIA.NativeDaemon 1.0
import PIA.NativeHelpers 1.0

QtObject {
  readonly property bool vpnEnabled: NativeDaemon.state.vpnEnabled
  readonly property string connectionState: NativeDaemon.state.connectionState
  readonly property int usingSlowInterval: NativeDaemon.state.usingSlowInterval
  readonly property bool needsReconnect: NativeDaemon.state.needsReconnect
  readonly property double bytesReceived: NativeDaemon.state.bytesReceived
  readonly property double bytesSent: NativeDaemon.state.bytesSent
  readonly property int forwardedPort: NativeDaemon.state.forwardedPort
  readonly property string externalIp: NativeDaemon.state.externalIp
  readonly property string externalVpnIp: NativeDaemon.state.externalVpnIp
  readonly property var chosenTransport: NativeDaemon.state.chosenTransport
  readonly property var actualTransport: NativeDaemon.state.actualTransport
  readonly property var vpnLocations: NativeDaemon.state.vpnLocations
  readonly property var shadowsocksLocations: NativeDaemon.state.shadowsocksLocations
  readonly property var connectingConfig: NativeDaemon.state.connectingConfig
  readonly property var connectedConfig: NativeDaemon.state.connectedConfig
  readonly property var nextConfig: NativeDaemon.state.nextConfig
  readonly property var connectedServer: NativeDaemon.state.connectedServer
  readonly property var availableLocations: NativeDaemon.state.availableLocations
  readonly property var groupedLocations: NativeDaemon.state.groupedLocations
  readonly property var openvpnUdpPortChoices: NativeDaemon.state.openvpnUdpPortChoices
  readonly property var openvpnTcpPortChoices: NativeDaemon.state.openvpnTcpPortChoices
  readonly property var intervalMeasurements: NativeDaemon.state.intervalMeasurements
  readonly property double connectionTimestamp: NativeDaemon.state.connectionTimestamp
  readonly property var overridesFailed: NativeDaemon.state.overridesFailed
  readonly property var overridesActive: NativeDaemon.state.overridesActive
  readonly property double openVpnAuthFailed: NativeDaemon.state.openVpnAuthFailed
  readonly property double connectionLost: NativeDaemon.state.connectionLost
  readonly property double proxyUnreachable: NativeDaemon.state.proxyUnreachable
  readonly property bool killswitchEnabled: NativeDaemon.state.killswitchEnabled
  readonly property string availableVersion: NativeDaemon.state.availableVersion
  readonly property int updateDownloadProgress: NativeDaemon.state.updateDownloadProgress
  readonly property string updateInstallerPath: NativeDaemon.state.updateInstallerPath
  readonly property double updateDownloadFailure: NativeDaemon.state.updateDownloadFailure
  readonly property string updateVersion: NativeDaemon.state.updateVersion
  readonly property double dnsConfigFailed: NativeDaemon.state.dnsConfigFailed
  readonly property bool tapAdapterMissing: NativeDaemon.state.tapAdapterMissing
  readonly property bool wintunMissing: NativeDaemon.state.wintunMissing
  readonly property string netExtensionState: NativeDaemon.state.netExtensionState
  readonly property bool connectionProblem: NativeDaemon.state.connectionProblem
  readonly property bool invalidClientExit: NativeDaemon.state.invalidClientExit
  readonly property bool killedClient: NativeDaemon.state.killedClient
  readonly property double hnsdFailing: NativeDaemon.state.hnsdFailing
  readonly property double hnsdSyncFailure: NativeDaemon.state.hnsdSyncFailure
  readonly property string originalGatewayIp: NativeDaemon.state.originalGatewayIp
  readonly property string originalInterfaceIp: NativeDaemon.state.originalInterfaceIp
  readonly property string originalInterface: NativeDaemon.state.originalInterface
  readonly property double snoozeEndTime: NativeDaemon.state.snoozeEndTime
  readonly property var splitTunnelSupportErrors: NativeDaemon.state.splitTunnelSupportErrors
  readonly property string tunnelDeviceName: NativeDaemon.state.tunnelDeviceName
  readonly property string tunnelDeviceLocalAddress: NativeDaemon.state.tunnelDeviceLocalAddress
  readonly property string tunnelDeviceRemoteAddress: NativeDaemon.state.tunnelDeviceRemoteAddress
  readonly property bool wireguardAvailable: NativeDaemon.state.wireguardAvailable
  readonly property bool wireguardKernelSupport: NativeDaemon.state.wireguardKernelSupport

  // Constants for special values of forwardedPort (see PortForwarder::Special)
  readonly property var portForward: {
    'inactive': 0,
    'attempting': -1,
    'failed': -2,
    'unavailable': -3
  }

  // Location geographic coordinates.  Not currently provided by servers lists.
  readonly property var locationCoords: {
    "ad": {lat: 42.506287, long: 1.521801},
    "ae": {lat: 23.424076, long: 53.847818},
    "al": {lat: 41.33165, long: 19.8318},
    "am": {lat: 40.179188, long: 44.499104},
    "ar": {lat: -38.416096, long: -63.616673},
    "aus": {lat: -33.868820, long: 151.209296},
    "aus_melbourne": {lat: -37.813628, long: 144.963058},
    "aus_perth": {lat: -31.950527, long: 115.860458},
    "austria": {lat: 47.516231, long: 14.550072},
    "ba": {lat: 43.858181, long: 18.412340},
    "bahamas": {lat: 25.03428, long: -77.39628},
    "bangladesh": {lat: 23.684994, long: 90.356331},
    "bd": {lat: 23.810331, long: 90.412521},
    "belgium": {lat: 50.503887, long: 4.469936},
    "bg": {lat: 42.655033, long: 25.231817},
    "br": {lat: -14.235004, long: -51.92528},
    "bs": {lat: 25.047983, long: -77.355415},
    "by": {lat: 27.561524, long: 53.904540},
    "ca": {lat: 45.501689, long: -73.567256},
    "cambodia": {lat: 12.565679, long: 104.990963},
    "ca_ontario": {lat: 51.253777, long: -85.232212},
    "ca_toronto": {lat: 43.653226, long: -79.383184},
    "ca_vancouver": {lat: 49.282729, long: -123.120738},
    "china": {lat: 35.86166, long: 104.195397},
    "cn": {lat: 114.057868, long: 22.543099},
    "cyprus": {lat: 35.126413, long: 33.429859},
    "cy": {lat: 33.382988, long: 35.188336},
    "czech": {lat: 50.075538, long: 14.4378},
    "de-frankfurt": {lat: 50.110922, long: 8.682127},
    "de_berlin": {lat: 52.520007, long: 13.404954},
    "denmark": {lat: 56.263920, long: 9.501785},
    "dz": {lat: 36.753769, long: 3.058756},
    "ee": {lat: 59.436962, long: 24.753574},
    "eg": {lat: 30.044420, long: 31.235712},
    "fi": {lat: 61.924110, long: 25.748151},
    "france": {lat: 46.227638, long: 2.213749},
    "ge": {lat: 41.716667, long: 44.783333},
    "georgia": {lat: 42.315407, long: 43.356892},
    "germany": {lat: 50.110922, long: 8.682127},
    "greenland": {lat: 71.706936, long: -42.604303	},
    "gl": {lat: 64.175000, long: -51.738889},
    "gr": {lat: 37.983810, long: 23.727539},
    "hk": {lat: 22.396428, long: 114.109497},
    "hr": {lat: 45.815399, long: 15.966568},
    "hungary": {lat: 47.162494, long: 19.503304},
    "im": {lat: 54.152337, long: -4.486123},
    "in": {lat: 20.593684, long: 78.96288},
    "ir": {lat: 35.689197, long: 51.388974},
    "iran": {lat: 32.427908, long: 53.688046},
    "ireland": {lat: 53.142367, long: -7.692054},
    "is": {lat: 64.852829, long: -18.301501},
    "israel": {lat: 31.046051, long: 34.851612},
    "italy": {lat: 41.871940, long: 12.56738},
    "japan": {lat: 36.204824, long: 138.252924},
    "kh": {lat: 104.892167, long: 11.544873},
    "kz": {lat: 51.160523, long: 71.470356},
    "kazakhstan": {lat: 48.019573, long: 66.923684},
    "li": {lat: 47.141370, long: 9.520700},
    "liechtenstein": {lat: 47.166, long: 9.555373},
    "lk": {lat: 6.927079, long: 79.861243},
    "lt": {lat: 54.687157, long: 25.279652},
    "lu": {lat: 49.815273, long: 6.129583},
    "lv": {lat: 56.946285, long: 24.105078},
    "ma": {lat: 33.971590, long: -6.849813},
    "macau": {lat: 22.198745, long:113.543873},
    "malta": {lat: 35.937496, long:14.375416},
    "man": {lat: 54.236107, long: -4.548056},
    "monaco": {lat: 43.750298, long: 7.412841},
    "mongolia": {lat: 46.862496, long: 103.846656},
    "montenegro": {lat: 42.708678, long: 19.37439},
    "morocco": {lat: 34.021992, long: -6.837620},
    "mc": {lat: 43.738418, long: 7.424616},
    "md": {lat: 47.265819, long: 28.598334},
    "me": {lat: 42.441286, long: 19.262892},
    "mexico": {lat: 23.634501, long: -102.552784},
    "mk": {lat: 41.608635, long: 21.745275},
    "mn": {lat: 47.920000, long: 106.920000},
    "mo": {lat: 22.198745, long: 113.543873},
    "mt": {lat: 35.898908, long: 14.514553},
    "mx": {lat: 23.634501, long: -102.552784},
    "my": {lat: 3.140853, long: 101.693207},
    "nigeria": {lat: 9.081999, long: 8.675277},
    "ng": {lat: 6.524379, long: 3.379206},
    "nl": {lat: 52.132633, long: 5.291266},
    "nl_amsterdam": {lat: 52.370216, long: 4.895168},
    "no": {lat: 60.472024, long: 8.468946},
    "nz": {lat: -40.900557, long: 174.885971},
    "pa": {lat: 8.983333, long: -79.516667},
    "panama": {lat: 8.537981, long: -80.782127},
    "poland": {lat: 51.919438, long: 19.145136},
    "philippines": {lat: 12.879721, long: 121.774017},
    "pt": {lat: 38.736946, long: -9.142685},
    "qatar": {lat: 25.354826, long: 51.183884},
    "qa": {lat: 25.291610, long: 51.530437},
    "ro": {lat: 45.943161, long: 24.96676},
    "rs": {lat: 44.016421, long: 21.005859},
    "ru": {lat: 59.934280, long: 30.335099},
    "saudiarabia": {lat: 23.885942, long: 45.079162},
    "sa": {lat: 24.713552, long: 46.675296},
    "sg": {lat: 1.352083, long: 103.819836},
    "si": {lat: 46.075219, long: 14.882733},
    "srilanka": {lat: 7.873054, long: 80.771797},
    "sk": {lat: 48.148598, long: 17.107748},
    "sofia": {lat: 42.697708, long: 23.321867},
    "spain": {lat: 40.463667, long: -3.74922},
    "sweden": {lat: 60.128161, long: 18.643501},
    "swiss": {lat: 46.818188, long: 8.227512},
    "tr": {lat: 38.963745, long: 35.243322},
    "tw": {lat: 25.032969, long: 121.565418},
    "taiwan": {lat: 23.69781, long: 120.960515},
    "ua": {lat: 48.379433, long: 31.165581},
    "uk": {lat: 51.507351, long: -0.127758},
    "uk_manchester": {lat: 53.480759, long: -2.242631},
    "uk_southampton": {lat: 50.909700, long: -1.404351},
    "us2": {lat: 36.414652, long: -77.739258},
    "us3": {lat: 40.607697, long: -120.805664},
    "us_atlanta": {lat: 33.748995, long: -84.387982},
    "us_california": {lat: 36.778261, long: -119.417932},
    "us_chicago": {lat: 41.878114, long: -87.629798},
    "us_denver": {lat: 39.739236, long: -104.990251},
    "us_florida": {lat: 27.664827, long: -81.515754},
    "us_houston": {lat: 29.760427, long: -95.369803},
    "us_las_vegas": {lat: 36.169941, long: -115.13983},
    "us_new_york_city": {lat: 40.712775, long: -74.005973},
    "us_seattle": {lat: 47.606209, long: -122.332071},
    "us_silicon_valley": {lat: 37.593392, long: -122.04383},
    "us_south_west": {lat: 33.623962, long: -109.654814},
    "us_washington_dc": {lat: 38.907192, long: -77.036871},
    "venezuela": {lat: 6.42375, long: -66.58973	},
    "vietnam": {lat: 14.058324, long: 108.277199},
    "yerevan": {lat: 40.069099, long: 45.038189},
    "ve": {lat: 10.469640, long: -66.803719},
    "vn": {lat: 21.027764, long: 105.834160},
    "za": {lat: -30.559482, long: 22.937506},
  }

  // Location names, so they can be translated, and translations shipped with
  // the app.
  //
  // These are all put in one big pool, _not_ associated with specific
  // locations, since they have moved around in some instances (especially in
  // the legacy-modern transition).  Location names have also become country
  // group names, and vice versa - country group names are also included in this
  // list.
  //
  // These are just marked with QT_TRANSLATE_NOOP, so the resulting array is
  // just all the English location names.  It's only used to see if a name from
  // the regions list is actually one that we know about and can translate.
  //
  // Also, note that Qt's doc calls the function "qsTranslateNoOp", but the name
  // is actually QT_TRANSLATE_NOOP per the code sample and
  // qqmlbuiltinfunctions.cpp (1801)
  readonly property var locationTranslatableNames: [
    QT_TRANSLATE_NOOP("DaemonData", "AU Melbourne"),
    QT_TRANSLATE_NOOP("DaemonData", "AU Perth"),
    QT_TRANSLATE_NOOP("DaemonData", "AU Sydney"),
    QT_TRANSLATE_NOOP("DaemonData", "Albania"),
    QT_TRANSLATE_NOOP("DaemonData", "Algeria"),
    QT_TRANSLATE_NOOP("DaemonData", "Andorra"),
    QT_TRANSLATE_NOOP("DaemonData", "Argentina"),
    QT_TRANSLATE_NOOP("DaemonData", "Armenia"),
    QT_TRANSLATE_NOOP("DaemonData", "Australia"),
    QT_TRANSLATE_NOOP("DaemonData", "Austria"),
    QT_TRANSLATE_NOOP("DaemonData", "Bahamas"),
    QT_TRANSLATE_NOOP("DaemonData", "Bangladesh"),
    QT_TRANSLATE_NOOP("DaemonData", "Belarus"),
    QT_TRANSLATE_NOOP("DaemonData", "Belgium"),
    QT_TRANSLATE_NOOP("DaemonData", "Bosnia and Herzegovina"),
    QT_TRANSLATE_NOOP("DaemonData", "Brazil"),
    QT_TRANSLATE_NOOP("DaemonData", "Bulgaria"),
    QT_TRANSLATE_NOOP("DaemonData", "CA Montreal"),
    QT_TRANSLATE_NOOP("DaemonData", "CA Ontario"),
    QT_TRANSLATE_NOOP("DaemonData", "CA Toronto"),
    QT_TRANSLATE_NOOP("DaemonData", "CA Vancouver"),
    QT_TRANSLATE_NOOP("DaemonData", "Cambodia"),
    QT_TRANSLATE_NOOP("DaemonData", "Canada"),
    QT_TRANSLATE_NOOP("DaemonData", "China"),
    QT_TRANSLATE_NOOP("DaemonData", "Cyprus"),
    QT_TRANSLATE_NOOP("DaemonData", "Croatia"),
    QT_TRANSLATE_NOOP("DaemonData", "Czech Republic"),
    QT_TRANSLATE_NOOP("DaemonData", "DE Berlin"),
    QT_TRANSLATE_NOOP("DaemonData", "DE Frankfurt"),
    QT_TRANSLATE_NOOP("DaemonData", "Denmark"),
    QT_TRANSLATE_NOOP("DaemonData", "Egypt"),
    QT_TRANSLATE_NOOP("DaemonData", "Estonia"),
    QT_TRANSLATE_NOOP("DaemonData", "Finland"),
    QT_TRANSLATE_NOOP("DaemonData", "France"),
    QT_TRANSLATE_NOOP("DaemonData", "Germany"),
    QT_TRANSLATE_NOOP("DaemonData", "Georgia"),
    QT_TRANSLATE_NOOP("DaemonData", "Greece"),
    QT_TRANSLATE_NOOP("DaemonData", "Greenland"),
    QT_TRANSLATE_NOOP("DaemonData", "Hong Kong"),
    QT_TRANSLATE_NOOP("DaemonData", "Hungary"),
    QT_TRANSLATE_NOOP("DaemonData", "Iceland"),
    QT_TRANSLATE_NOOP("DaemonData", "India"),
    QT_TRANSLATE_NOOP("DaemonData", "Iran"),
    QT_TRANSLATE_NOOP("DaemonData", "Ireland"),
    QT_TRANSLATE_NOOP("DaemonData", "Isle of Man"),
    QT_TRANSLATE_NOOP("DaemonData", "Israel"),
    QT_TRANSLATE_NOOP("DaemonData", "Italy"),
    QT_TRANSLATE_NOOP("DaemonData", "Japan"),
    QT_TRANSLATE_NOOP("DaemonData", "Kazakhstan"),
    QT_TRANSLATE_NOOP("DaemonData", "Latvia"),
    QT_TRANSLATE_NOOP("DaemonData", "Liechtenstein"),
    QT_TRANSLATE_NOOP("DaemonData", "Lithuania"),
    QT_TRANSLATE_NOOP("DaemonData", "Luxembourg"),
    // Spelling changed at one point, both seem to be acceptable so keep both
    // since we already translated the old one
    QT_TRANSLATE_NOOP("DaemonData", "Macao"),
    QT_TRANSLATE_NOOP("DaemonData", "Macau"),
    QT_TRANSLATE_NOOP("DaemonData", "Malaysia"),
    QT_TRANSLATE_NOOP("DaemonData", "Malta"),
    QT_TRANSLATE_NOOP("DaemonData", "Mexico"),
    QT_TRANSLATE_NOOP("DaemonData", "Moldova"),
    QT_TRANSLATE_NOOP("DaemonData", "Monaco"),
    QT_TRANSLATE_NOOP("DaemonData", "Mongolia"),
    QT_TRANSLATE_NOOP("DaemonData", "Montenegro"),
    QT_TRANSLATE_NOOP("DaemonData", "Morocco"),
    QT_TRANSLATE_NOOP("DaemonData", "Netherlands"),
    QT_TRANSLATE_NOOP("DaemonData", "New Zealand"),
    QT_TRANSLATE_NOOP("DaemonData", "Nigeria"),
    QT_TRANSLATE_NOOP("DaemonData", "North Macedonia"),
    QT_TRANSLATE_NOOP("DaemonData", "Norway"),
    QT_TRANSLATE_NOOP("DaemonData", "Panama"),
    QT_TRANSLATE_NOOP("DaemonData", "Philippines"),
    QT_TRANSLATE_NOOP("DaemonData", "Poland"),
    QT_TRANSLATE_NOOP("DaemonData", "Portugal"),
    QT_TRANSLATE_NOOP("DaemonData", "Qatar"),
    QT_TRANSLATE_NOOP("DaemonData", "Romania"),
    QT_TRANSLATE_NOOP("DaemonData", "Russia"),
    QT_TRANSLATE_NOOP("DaemonData", "Saudi Arabia"),
    QT_TRANSLATE_NOOP("DaemonData", "Serbia"),
    QT_TRANSLATE_NOOP("DaemonData", "Singapore"),
    QT_TRANSLATE_NOOP("DaemonData", "Slovakia"),
    QT_TRANSLATE_NOOP("DaemonData", "Slovenia"),
    QT_TRANSLATE_NOOP("DaemonData", "South Africa"),
    QT_TRANSLATE_NOOP("DaemonData", "Spain"),
    QT_TRANSLATE_NOOP("DaemonData", "Sri Lanka"),
    QT_TRANSLATE_NOOP("DaemonData", "Sweden"),
    QT_TRANSLATE_NOOP("DaemonData", "Switzerland"),
    QT_TRANSLATE_NOOP("DaemonData", "Taiwan"),
    QT_TRANSLATE_NOOP("DaemonData", "Turkey"),
    QT_TRANSLATE_NOOP("DaemonData", "UAE"),
    QT_TRANSLATE_NOOP("DaemonData", "UK London"),
    QT_TRANSLATE_NOOP("DaemonData", "UK Manchester"),
    QT_TRANSLATE_NOOP("DaemonData", "UK Southampton"),
    QT_TRANSLATE_NOOP("DaemonData", "US Atlanta"),
    QT_TRANSLATE_NOOP("DaemonData", "US California"),
    QT_TRANSLATE_NOOP("DaemonData", "US Chicago"),
    QT_TRANSLATE_NOOP("DaemonData", "US Denver"),
    QT_TRANSLATE_NOOP("DaemonData", "US East"),
    QT_TRANSLATE_NOOP("DaemonData", "US Florida"),
    QT_TRANSLATE_NOOP("DaemonData", "US Houston"),
    QT_TRANSLATE_NOOP("DaemonData", "US Las Vegas"),
    QT_TRANSLATE_NOOP("DaemonData", "US New York City"),
    QT_TRANSLATE_NOOP("DaemonData", "US New York"),
    QT_TRANSLATE_NOOP("DaemonData", "US Seattle"),
    QT_TRANSLATE_NOOP("DaemonData", "US Silicon Valley"),
    QT_TRANSLATE_NOOP("DaemonData", "US Texas"),
    QT_TRANSLATE_NOOP("DaemonData", "US Washington DC"),
    QT_TRANSLATE_NOOP("DaemonData", "US West"),
    QT_TRANSLATE_NOOP("DaemonData", "Ukraine"),
    QT_TRANSLATE_NOOP("DaemonData", "United Arab Emirates"),
    QT_TRANSLATE_NOOP("DaemonData", "United Kingdom"),
    QT_TRANSLATE_NOOP("DaemonData", "United States"),
    QT_TRANSLATE_NOOP("DaemonData", "Venezuela"),
    QT_TRANSLATE_NOOP("DaemonData", "Vietnam"),
  ]

  // Put the translatable names in an object (as a crude map) to avoid iterating
  // through the list just to sanity-check each name as it's translated
  readonly property var locationNames: {
    var names = {}
    for(var i=0; i<locationTranslatableNames.length; ++i) {
      names[locationTranslatableNames[i]] = true
    }
    return names
  }

  // The regions list just gives us country codes, not names.  Map the codes to
  // names here.  The translatable names appear in the list above.
  readonly property var countryNames: {
    "de": "Germany",
    "ca": "Canada",
    "us": "United States",
    "au": "Australia",
    "gb": "United Kingdom"
  }

  function haveTranslatableName(name) {
    return !!locationNames[name]
  }

  // Translate a location name.  Used by Daemon.getLocationName() and
  // Daemon.getCountryName() - just fills in the correct context for uiTranslate().
  function translateName(name) {
    // Make sure the name is one we're aware of.  Locations can be added and
    // renamed, so we may get names here that we didn't know about when the app
    // was shipped.
    //
    // If a location is renamed to a name that we haven't translated, it will
    // display the new English name, and the old translations will not be used.
    // This is intentional to handle cases like "Germany" becoming
    // "DE Frankfurt", which happened when "DE Berlin" was added - without this
    // check, we would have displayed "DE" -> "Germany", "DE Berlin" when the
    // regions were updated.
    //
    // It would _probably_ be fine to skip this check, but this is best for
    // robustness:
    // - makes sure we don't rely on any particular Qt behavior when trying to
    //   translate a string that doens't exist in translation files
    // - makes sure we don't accidentally use some non-location-name string that
    //   might appear in the DaemonData context somehow
    if(haveTranslatableName(name))
      return uiTranslate("DaemonData", name)
    return name // Name not known
  }

  function getBestLocationForCountry(countryCode) {
    return NativeHelpers.getBestLocationForCountry(NativeDaemon.state, countryCode)
  }

  // Check location metadata and log diagnostics for devs - such as new
  // locations that should be added, old locations that might be removable,
  // name changes, etc.
  function checkLocationMetadata(groupedLocations) {
    var loc, meta
    // Log if any locations in the server list are missing metadata that have to
    // be manually added.
    for(var locId in availableLocations) {
      loc = availableLocations[locId]
      if(!locationCoords[locId]) {
        console.info('Location ' + loc.name + ' (' + locId + ') does not have coordinates')
      }

      if(!haveTranslatableName(loc.name))
        console.info('Location ' + loc.name + ' (' + locId + ') does not have name translations')
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
