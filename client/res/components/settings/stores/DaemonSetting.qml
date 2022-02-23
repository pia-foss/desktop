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

import QtQuick 2.0
import "../../daemon"
import "."

Setting {
  // The name of a setting in Daemon.settings
  property string name
  
  // Override the apparent value of the setting.  When 'override' is true,
  // 'sourceValue' becomes 'overrideValue' instead of the actual setting value.
  //
  // The control using the setting should be disabled when the override is
  // active.  (Changes in currentValue in this state would still be applied, but
  // would not cause sourceValue to change until the override is disabled.)
  //
  // This is usually used for settings that depend on other settings.  When a
  // setting is unavailable, typically the setting is disabled, and an override
  // value is enabled to display the effective value in the UI.
  property bool override: false
  property var overrideValue: null

  sourceValue: override ? overrideValue : Daemon.settings[name]

  // A signal that the daemon value has been updated
  signal applyFinished;

  onCurrentValueChanged: {
    if(override)
      return;

    if (name && currentValue !== undefined && currentValue !== Daemon.settings[name]) {
      var param = {};
      param[name] = currentValue;
      Daemon.applySettings(param, function(error) {
        if (error) {
          console.error("Error applying setting " + name + ":", error);
          currentValue = Daemon.settings[name];
        }
        else {
          applyFinished(currentValue);
        }
      });
    }
  }
}
