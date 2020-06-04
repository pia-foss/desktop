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
  readonly property bool invalidClientExit: NativeDaemon.state.invalidClientExit
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

  // Location metadata - info we need about the locations, but that isn't
  // provided by the server list.
  // Currently contains:
  // - Latitude/longitude
  // - Translatable name
  //
  // Note that the names here are just marked with QT_TRANSLATE_NOOP, they are not
  // actually translated in the metadata.  Daemon.getLocationName() checks if
  // the name from the server list still matches the name that was translated,
  // and only uses the translation if it does.  (This handles cases like
  // "Germany" becoming "DE Frankfurt", which happened when "DE Berlin" was
  // added - without this check, we would have displayed
  // "DE" -> "Germany", "DE Berlin" when the regions were updated.)
  //
  // Also, note that Qt's doc calls the function "qsTranslateNoOp", but the name is
  // actually QT_TRANSLATE_NOOP per the code sample and qqmlbuiltinfunctions.cpp (1801)
  readonly property var locationMetadata: {
    "ae": {lat: 23.424076, long: 53.847818, name: QT_TRANSLATE_NOOP("DaemonData", "UAE")},
    "al": {lat: 41.33165, long: 19.8318, name: QT_TRANSLATE_NOOP("DaemonData", "Albania")},
    "ar": {lat: -38.416096, long: -63.616673, name: QT_TRANSLATE_NOOP("DaemonData", "Argentina")},
    "aus": {lat: -33.868820, long: 151.209296, name: QT_TRANSLATE_NOOP("DaemonData", "AU Sydney")},
    "aus_melbourne": {lat: -37.813628, long: 144.963058, name: QT_TRANSLATE_NOOP("DaemonData", "AU Melbourne")},
    "aus_perth": {lat: -31.950527, long: 115.860458, name: QT_TRANSLATE_NOOP("DaemonData", "AU Perth")},
    "austria": {lat: 47.516231, long: 14.550072, name: QT_TRANSLATE_NOOP("DaemonData", "Austria")},
    "ba": {lat: 43.858181, long: 18.412340, name: QT_TRANSLATE_NOOP("DaemonData", "Bosnia and Herzegovina")},
    "belgium": {lat: 50.503887, long: 4.469936, name: QT_TRANSLATE_NOOP("DaemonData", "Belgium")},
    "bg": {lat: 42.655033, long: 25.231817, name: QT_TRANSLATE_NOOP("DaemonData", "Bulgaria")},
    "brazil": {lat: -14.235004, long: -51.92528, name: QT_TRANSLATE_NOOP("DaemonData", "Brazil")},
    "ca": {lat: 45.501689, long: -73.567256, name: QT_TRANSLATE_NOOP("DaemonData", "CA Montreal")},
    "ca_ontario": {lat: 51.253777, long: -85.232212, name: QT_TRANSLATE_NOOP("DaemonData", "CA Ontario")},
    "ca_toronto": {lat: 43.653226, long: -79.383184, name: QT_TRANSLATE_NOOP("DaemonData", "CA Toronto")},
    "ca_vancouver": {lat: 49.282729, long: -123.120738, name: QT_TRANSLATE_NOOP("DaemonData", "CA Vancouver")},
    "czech": {lat: 50.075538, long: 14.4378, name: QT_TRANSLATE_NOOP("DaemonData", "Czech Republic")},
    "de_berlin": {lat: 52.520007, long: 13.404954, name: QT_TRANSLATE_NOOP("DaemonData", "DE Berlin")},
    "denmark": {lat: 56.263920, long: 9.501785, name: QT_TRANSLATE_NOOP("DaemonData", "Denmark")},
    "ee": {lat: 59.436962, long: 24.753574, name: QT_TRANSLATE_NOOP("DaemonData", "Estonia")},
    "fi": {lat: 61.924110, long: 25.748151, name: QT_TRANSLATE_NOOP("DaemonData", "Finland")},
    "france": {lat: 46.227638, long: 2.213749, name: QT_TRANSLATE_NOOP("DaemonData", "France")},
    "germany": {lat: 50.110922, long: 8.682127, name: QT_TRANSLATE_NOOP("DaemonData", "DE Frankfurt")}, //Frankfurt
    "gr": {lat: 37.983810, long: 23.727539, name: QT_TRANSLATE_NOOP("DaemonData", "Greece")},
    "hk": {lat: 22.396428, long: 114.109497, name: QT_TRANSLATE_NOOP("DaemonData", "Hong Kong")},
    "hr": {lat: 45.815399, long: 15.966568, name: QT_TRANSLATE_NOOP("DaemonData", "Croatia")},
    "hungary": {lat: 47.162494, long: 19.503304, name: QT_TRANSLATE_NOOP("DaemonData", "Hungary")},
    "in": {lat: 20.593684, long: 78.96288, name: QT_TRANSLATE_NOOP("DaemonData", "India")},
    "ireland": {lat: 53.142367, long: -7.692054, name: QT_TRANSLATE_NOOP("DaemonData", "Ireland")},
    "is": {lat: 64.852829, long: -18.301501, name: QT_TRANSLATE_NOOP("DaemonData", "Iceland")},
    "israel": {lat: 31.046051, long: 34.851612, name: QT_TRANSLATE_NOOP("DaemonData", "Israel")},
    "italy": {lat: 41.871940, long: 12.56738, name: QT_TRANSLATE_NOOP("DaemonData", "Italy")},
    "japan": {lat: 36.204824, long: 138.252924, name: QT_TRANSLATE_NOOP("DaemonData", "Japan")},
    "lt": {lat: 54.687157, long: 25.279652, name: QT_TRANSLATE_NOOP("DaemonData", "Lithuania")},
    "lu": {lat: 49.815273, long: 6.129583, name: QT_TRANSLATE_NOOP("DaemonData", "Luxembourg")},
    "lv": {lat: 56.946285, long: 24.105078, name: QT_TRANSLATE_NOOP("DaemonData", "Latvia")},
    "md": {lat: 47.265819, long: 28.598334, name: QT_TRANSLATE_NOOP("DaemonData", "Moldova")},
    "mexico": {lat: 23.634501, long: -102.552784, name: QT_TRANSLATE_NOOP("DaemonData", "Mexico")},
    "mk": {lat: 41.608635, long: 21.745275, name: QT_TRANSLATE_NOOP("DaemonData", "North Macedonia")},
    "my": {lat: 3.140853, long: 101.693207, name: QT_TRANSLATE_NOOP("DaemonData", "Malaysia")},
    "nl": {lat: 52.132633, long: 5.291266, name: QT_TRANSLATE_NOOP("DaemonData", "Netherlands")},
    "no": {lat: 60.472024, long: 8.468946, name: QT_TRANSLATE_NOOP("DaemonData", "Norway")},
    "nz": {lat: -40.900557, long: 174.885971, name: QT_TRANSLATE_NOOP("DaemonData", "New Zealand")},
    "poland": {lat: 51.919438, long: 19.145136, name: QT_TRANSLATE_NOOP("DaemonData", "Poland")},
    "pt": {lat: 38.736946, long: -9.142685, name: QT_TRANSLATE_NOOP("DaemonData", "Portugal")},
    "ro": {lat: 45.943161, long: 24.96676, name: QT_TRANSLATE_NOOP("DaemonData", "Romania")},
    "rs": {lat: 44.016421, long: 21.005859, name: QT_TRANSLATE_NOOP("DaemonData", "Serbia")},
    "sg": {lat: 1.352083, long: 103.819836, name: QT_TRANSLATE_NOOP("DaemonData", "Singapore")},
    "si": {lat: 46.075219, long: 14.882733, name: QT_TRANSLATE_NOOP("DaemonData", "Slovenia")},
    "sk": {lat: 48.148598, long: 17.107748, name: QT_TRANSLATE_NOOP("DaemonData", "Slovakia")},
    "spain": {lat: 40.463667, long: -3.74922, name: QT_TRANSLATE_NOOP("DaemonData", "Spain")},
    "sweden": {lat: 60.128161, long: 18.643501, name: QT_TRANSLATE_NOOP("DaemonData", "Sweden")},
    "swiss": {lat: 46.818188, long: 8.227512, name: QT_TRANSLATE_NOOP("DaemonData", "Switzerland")},
    "tr": {lat: 38.963745, long: 35.243322, name: QT_TRANSLATE_NOOP("DaemonData", "Turkey")},
    "ua": {lat: 48.379433, long: 31.165581, name: QT_TRANSLATE_NOOP("DaemonData", "Ukraine")},
    "uk": {lat: 51.507351, long: -0.127758, name: QT_TRANSLATE_NOOP("DaemonData", "UK London")}, // London
    "uk_manchester": {lat: 53.480759, long: -2.242631, name: QT_TRANSLATE_NOOP("DaemonData", "UK Manchester")},
    "uk_southampton": {lat: 50.909700, long: -1.404351, name: QT_TRANSLATE_NOOP("DaemonData", "UK Southampton")},
    "us2": {lat: 36.414652, long: -77.739258, name: QT_TRANSLATE_NOOP("DaemonData", "US East")}, // East
    "us3": {lat: 40.607697, long: -120.805664, name: QT_TRANSLATE_NOOP("DaemonData", "US West")},
    "us_atlanta": {lat: 33.748995, long: -84.387982, name: QT_TRANSLATE_NOOP("DaemonData", "US Atlanta")},
    "us_california": {lat: 36.778261, long: -119.417932, name: QT_TRANSLATE_NOOP("DaemonData", "US California")},
    "us_chicago": {lat: 41.878114, long: -87.629798, name: QT_TRANSLATE_NOOP("DaemonData", "US Chicago")},
    "us_denver": {lat: 39.739236, long: -104.990251, name: QT_TRANSLATE_NOOP("DaemonData", "US Denver")},
    "us_florida": {lat: 27.664827, long: -81.515754, name: QT_TRANSLATE_NOOP("DaemonData", "US Florida")},
    "us_houston": {lat: 29.760427, long: -95.369803, name: QT_TRANSLATE_NOOP("DaemonData", "US Houston")},
    "us_las_vegas": {lat: 36.169941, long: -115.13983, name: QT_TRANSLATE_NOOP("DaemonData", "US Las Vegas")},
    "us_new_york_city": {lat: 40.712775, long: -74.005973, name: QT_TRANSLATE_NOOP("DaemonData", "US New York City")},
    "us_seattle": {lat: 47.606209, long: -122.332071, name: QT_TRANSLATE_NOOP("DaemonData", "US Seattle")},
    "us_silicon_valley": {lat: 37.593392, long: -122.04383, name: QT_TRANSLATE_NOOP("DaemonData", "US Silicon Valley")},
    "us_south_west": {lat: 33.623962, long: -109.654814, name: QT_TRANSLATE_NOOP("DaemonData", "US Texas")},
    "us_washington_dc": {lat: 38.907192, long: -77.036871, name: QT_TRANSLATE_NOOP("DaemonData", "US Washington DC")},
    "za": {lat: -30.559482, long: 22.937506, name: QT_TRANSLATE_NOOP("DaemonData", "South Africa")}
  }

  // The regions list doesn't give us country names either.  We only need them
  // for countries with more than one region.
  readonly property var countryNames: {
    "de": QT_TRANSLATE_NOOP("DaemonData", "Germany"),
    "ca": QT_TRANSLATE_NOOP("DaemonData", "Canada"),
    "us": QT_TRANSLATE_NOOP("DaemonData", "United States"),
    "au": QT_TRANSLATE_NOOP("DaemonData", "Australia"),
    "gb": QT_TRANSLATE_NOOP("DaemonData", "United Kingdom")
  }

  // Translate a location name.  Used by Daemon.getLocationName() and
  // Daemon.getCountryName() - just fills in the correct context for uiTranslate().
  function translateName(name) {
    return uiTranslate("DaemonData", name)
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
    if(availableLocations.length > 0) {
      for(var metaId in locationMetadata) {
        if(!availableLocations[metaId]) {
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
