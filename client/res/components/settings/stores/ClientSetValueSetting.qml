// Copyright (c) 2024 Private Internet Access, Inc.
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
import "../../../javascript/util.js" as Util

// ClientSetValueSetting is a boolean setting that represents whether a
// particular value is in a set of values from Client.settings.
//
// The setting's value indicates whether settingValue is currently in the
// setting identified by name.  Changing currentValue adds the value to the set
// or removes it.  When the value is added, it's added to the end of the array.
//
// (The set is actually represented with a JS Array since it's passed through as
// JSON.)
Setting {
  // The name of the setting in Client.settings
  property string name
  // The value to find in that setting
  property string settingValue

  sourceValue: {
    if(!Client.settings[name])
      return false
    return Util.arrayIncludes(Client.settings[name], settingValue)
  }

  function addValue() {
    var currentValues = Client.settings[name] || []
    if(!Util.arrayIncludes(currentValues, settingValue)) {
      var newSettings = {}
      newSettings[name] = currentValues.concat([settingValue])
      if(!Client.applySettings(newSettings)) {
        console.error("Can't add value " + settingValue + " to setting " + name,
                      error)
        currentValue = sourceValue
      }
    }
  }

  function removeValue() {
    var currentValues = Client.settings[name] || []
    if(Util.arrayIncludes(currentValues, settingValue)) {
      var newSettings = {}
      newSettings[name] = currentValues.filter(function(item){return item !== settingValue})
      if(!Client.applySettings(newSettings)) {
        console.error("Can't remove value " + settingValue + " from setting " + name,
                      error)
        currentValue = sourceValue
      }
    }
  }

  onCurrentValueChanged: {
    if(currentValue)
      addValue()
    else
      removeValue()
  }
}
