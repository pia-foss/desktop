// Copyright (c) 2021 Private Internet Access, Inc.
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
import PIA.NativeHelpers 1.0
import "../core"
import "../theme"

// Image displaying the flag for a particular country.  Set the "country"
// property to the 2-character code for the country.
//
// If there isn't a resource for that country's flag (including if the country
// is not valid, etc.), this displays a default placeholder.
Image {
  property string countryCode
  property bool offline: false

  source: flagFileFromCountry(countryCode)
  height: 16
  width: 24

  function flagFileFromCountry(country) {
    if(!offline)
        var flagFile = "qrc:/img/flags/" + country.toLowerCase() + ".png"
    else
        var flagFile = "qrc:/img/flags/" + country.toLowerCase() + "-offline.png"
    if(NativeHelpers.resourceExists(flagFile))
      return flagFile
    // TODO: Need an "unknown" flag/symbol; use this one instead for now
    return "qrc:/img/flags/un.png"
  }
}
