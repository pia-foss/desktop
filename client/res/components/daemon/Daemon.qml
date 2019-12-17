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

pragma Singleton
import QtQuick 2.0
import PIA.NativeDaemon 1.0
import "../../javascript/util.js" as Util

// This is a QML shim for DaemonInterface
// Marshalling properties from C++ to QML is too slow
// Using this proxy object is much faster
QtObject {
  readonly property DaemonData data: DaemonData {}
  readonly property DaemonAccount account: DaemonAccount {}
  readonly property DaemonSettings settings: DaemonSettings {}
  readonly property DaemonState state: DaemonState {}

  // Boolean determining whether or not the daemon is connected to the client
  readonly property bool connected: NativeDaemon.connected

  // The signal connections made to QmlCallResults in call() do not keep the
  // QmlCallResult alive on their own.  We have to put these QmlCallResults
  // somewhere to prevent them from being garbage collected.
  //
  // Note that this behavior depends on whether the QML debugger is connected,
  // as the QML debugger itself will cause the result to remain alive.  Without
  // this hack, and without the QML debugger, the callback may not be called if
  // the GC gets a chance to run, since result is destroyed and the connections
  // are broken.
  //
  // On top of that, V4 does not support Set, so we have to use a crude
  // approximation with an object used as a map.
  readonly property var pendingResults: new Util.RefHolder()

  function call(method, params, cb) {
    if (!params) {
      params = [];
    } else if (!Array.isArray(params)) {
      params = Array.prototype.slice.call(params);
    }
    if (cb === undefined && params.length > 0 && typeof params[params.length - 1] === "function") {
      cb = params.pop();
    }
    if (typeof cb === "function") {
      var result = NativeDaemon.qmlCall(method, params);
      // These signal connections will not keep the result alive on their own,
      // we have to put the result somewhere to keep it alive and ensure that
      // the callback is called.
      pendingResults.storeRef(result)
      result.resolved.connect(function(value) {
        pendingResults.releaseRef(result)
        cb(undefined, value)
      })
      result.rejected.connect(function(nativeError) {
        pendingResults.releaseRef(result)

        var errMsg = "[%1][%2][%3]: %4"
          .arg(nativeError.file())
          .arg(nativeError.line())
          .arg(nativeError.code())
          .arg(nativeError.errorString())

        var errObj = new Error(errMsg)
        errObj.code = nativeError.code()
        cb(errObj)
      })
    } else {
      NativeDaemon.post(method, params);
    }
  }

  function applySettings(settings, reconnectIfNeeded) {
    call("applySettings", arguments);
  }
  function resetSettings(settings) {
    call("resetSettings", arguments);
  }
  function login(username, password) {
    call("login", arguments);
  }
  function logout() {
    call("logout", arguments);
  }
  function connectVPN() {
    call("connectVPN", arguments);
  }
  function disconnectVPN() {
    call("disconnectVPN", arguments);
  }
  function writeDiagnostics () {
    call("writeDiagnostics", arguments);
  }
  function refreshUpdate() {
    call("refreshUpdate", arguments)
  }
  function downloadUpdate() {
    call("downloadUpdate", arguments);
  }
  function cancelDownloadUpdate() {
    call("cancelDownloadUpdate", arguments);
  }
  function writeDummyLogs() {
    call("writeDummyLogs", arguments);
  }
  function crash() {
    call ("crash", arguments);
  }
  function installKext() {
    call ("installKext", arguments);
  }
  function startSnooze(timeout) {
    call ("startSnooze", arguments);
  }
  function stopSnooze() {
    call ("stopSnooze", arguments);
  }

  // Get the name of a location.
  // Returns the location's localized name if possible, otherwise the name
  // provided by the server, or its ID as a last resort.
  function getLocationName(loc) {
    var locMeta = data.locationMetadata[loc.id]
    if(locMeta && loc.name && loc.name === locMeta.name) {
      // The translatable name is known and the server name still matches the
      // name that was translated.  Return a localized name.
      return data.translateName(locMeta.name)
    }
    // The location has no metadata, its name has changed, or the server did not
    // provide a name, etc.  Return the server name if it exists or the ID
    // otherwise.
    return loc.name || loc.id
  }

  // Get the name of a location from a location ID.
  function getLocationIdName(locId) {
    var loc = data.locations[locId]
    return loc ? getLocationName(loc) : locId
  }

  // Get the localized name of a country by country code.
  function getCountryName(countryCode) {
    var countryName = data.countryNames[countryCode.toLowerCase()]
    if(countryName)
      return data.translateName(countryName)
    // Fall back to displaying the capitalized country code if no country name
    // is known
    return countryCode.toUpperCase()
  }
}
