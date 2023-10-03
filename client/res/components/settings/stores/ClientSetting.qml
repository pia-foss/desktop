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

import QtQuick 2.0
import "../../client"

Setting {
  // The name of a setting in Client.settings
  property string name

  sourceValue: Client.settings[name]

  onCurrentValueChanged: {
    if(currentValue !== Client.settings[name]) {
      var newSettings = {}
      newSettings[name] = currentValue
      if(!Client.applySettings(newSettings)) {
        console.error("Error applying setting " + JSON.stringify(newSettings))
        currentValue = sourceValue
      }
    }
  }
}
