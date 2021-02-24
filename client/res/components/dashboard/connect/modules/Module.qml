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
import "../../../common"
import "../../../core"
import "../../../theme"

// Module provides core functionality of all modules (both movable modules and
// the separator module).
//
// Module works with ModuleSorter to set up the modules.  In particular, Module
// computes its own Y position.  (It's a bit simpler to include this logic here
// than have ModuleSorter bind it into all modules.)
//
// The module implementation should specify an implicitHeight, which should not
// be animated.
Rectangle {
  id: module

  // Key string used by the module - the ID of the module in the module lists.
  property string moduleKey

  // Key constant for the separator module
  readonly property string separatorKey: "separator"

  // Data provided by ModuleSorter - grouped into an object to simplify bindings
  // from ModuleSorter.  Some properties are used by MovableModule; shouldn't
  // generally be used by specific modules.
  property QtObject moduleData

  // Whether this module is above the fold
  readonly property bool isAboveFold: {
    if(!moduleData)
      return false

    for(var i=0; i<moduleData.moduleList.length; ++i) {
      // If the separator is above this module, it's below the fold
      // (including the separator module itself)
      if(moduleData.moduleList[i] === separatorKey)
        return false
      // If this module is above the separator, it's above the fold
      if(moduleData.moduleList[i] === moduleKey)
        return true
    }
    // Shouldn't reach this as long as the module list contains all module keys
    console.warn("Failed to find module key: " + moduleKey)
    return false
  }

  // Compute the Y position for this module based on the prior modules' heights
  readonly property real computedY: {
    if(!moduleData)
      return 0

    var y=0
    for(var i=0; i<moduleData.moduleList.length; ++i) {
      var nextModuleKey = moduleData.moduleList[i]
      if(nextModuleKey === moduleKey)
        return y
      var nextModule = moduleData.modules[nextModuleKey]
      // Add the next module's height - use implicitHeight so animations on
      // height don't affect this computation
      y += nextModule.implicitHeight
    }
    // Shouldn't reach this as long as the module list contains all module keys
    console.warn("Failed to find module key: " + moduleKey)
    return y
  }

  // MovableModule interacts with (and overrides) some of the positioning
  // bindings here.
  width: parent.width
  y: computedY

  color: Theme.dashboard.backgroundColor

  Behavior on y {
    id: yBehavior
    SmoothedAnimation {
      id: yAnimation
      duration: Theme.animation.quickDuration
    }
  }

  // Set this to false to disable the Y animation.  (Used by MovableModule for
  // a module being toggled with the bookmark button.)
  property alias yAnimationEnabled: yBehavior.enabled

  // Whether a Y animation is running right now
  readonly property alias yAnimationRunning: yAnimation.running

  // Hide below-fold content when ModuleSorter says to
  visible: isAboveFold || (moduleData ? moduleData.showBelowFold : false)
}
