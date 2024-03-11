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

import QtQuick 2.9
import "../../client"
import "../../common"
import "../../core"
import "../../theme"
import "./modules"
import "./modules/settings"
import PIA.NativeHelpers 1.0

Item {
  id: moduleSorter

  // Whether to show bookmark icons on the modules
  property bool showBookmarks

  // Whether to show content that's below the fold
  property bool showBelowFold

  // Height of the above-fold section only
  readonly property int aboveFoldHeight: separatorModule.computedY

  // Unified module list, including the separator module
  readonly property var moduleList: {
    var moduleList = [].concat(Client.settings.primaryModules,
                               [separatorModule.moduleKey],
                               Client.settings.secondaryModules)
    // sanitizeModuleList() handles corrupted settings, new modules added in
    // later versions, and transients during drags due to the module list being
    // stored in two separate setting values.
    return sanitizeModuleList(moduleList)
  }

  // Map of modules by module key
  readonly property var modules: {
    var modMap = {}
    for(var i=0; i<moduleSorter.children.length; ++i) {
      if(moduleSorter.children[i] && moduleSorter.children[i].moduleKey)
        modMap[moduleSorter.children[i].moduleKey] = moduleSorter.children[i]
    }
    return modMap
  }

  // How much to extend the right-side clip bound when dragging a module.
  // Normally, ConnectPage and DashboardWrapper clip their contents for various
  // reasons; when dragging a module it appears to "pop out" of the dashboard,
  // and the clip bounds are temporarily extended.
  readonly property real clipRightExtend: {
    var sorterRight = moduleSorter.width
    var module, moduleRight, maxModuleRight = sorterRight
    for(var moduleKey in modules) {
      module = modules[moduleKey]
      moduleRight = module.x + module.width
      maxModuleRight = Math.max(maxModuleRight, moduleRight)
    }
    return maxModuleRight - sorterRight
  }

  // Apply a new module list (split it up and apply it to ClientSettings)
  function applyModuleList(moduleKeys) {
    var separatorIdx = moduleKeys.indexOf(separatorModule.moduleKey)
    var primaries = moduleKeys.slice(0, separatorIdx)
    var secondaries = moduleKeys.slice(separatorIdx+1, moduleKeys.length)

    Client.applySettings({primaryModules: primaries, secondaryModules: secondaries})
  }

  implicitHeight: {
    // The height is the sum of all module heights
    var height = 0
    for(var i=0; i<moduleList.length; ++i) {
      // Use implicitHeight in case there is an animation on height
      var module = modules[moduleList[i]]
      if(module)
        height += module.implicitHeight
    }
    return height
  }

  // Data provided to all modules.  (Wrapped mainly to simplify bindings below.)
  QtObject {
    id: moduleData
    // Whether to show bookmark icons
    readonly property bool showBookmarks: moduleSorter.showBookmarks
    // Whether to show content below the fold
    readonly property bool showBelowFold: moduleSorter.showBelowFold
    // Unified module list (allows modules to determine whether they are in the
    // above-fold section)
    readonly property var moduleList: moduleSorter.moduleList
    // Map of modules by module key (allows modules to compute their Y positions)
    readonly property var modules: moduleSorter.modules

    // Apply a new module list (used by modules to move themselves for keyboard
    // navigation)
    function applyModuleList(moduleList) {moduleSorter.applyModuleList(moduleList)}

    // Called when a module repositions during a drag to check if the module
    // order should change.  Updates the client settings if necessary.
    function checkModuleOrder(movingModuleKey) {
      // Compute the bottom positions of all the other modules as if the moving
      // module did not exist.  This acts as if the user is dragging a line to
      // be inserted between the other modules.
      var moduleBottoms = []
      var nextY = 0
      var currentMovingIdx  // Current index of the moving module
      var i
      for(i=0; i<moduleList.length; ++i) {
        if(moduleList[i] === movingModuleKey) {
          currentMovingIdx = i
        }
        else {
          var module = modules[moduleList[i]]
          nextY += module.implicitHeight
          moduleBottoms.push({key: moduleList[i], y: nextY})
        }
      }
      // Determine the moving module's Y position and where to insert it in the
      // array
      var movingModule = modules[movingModuleKey]
      var movingY = movingModule.y
      // Insert it at the position that's nearest to movingY.  The first
      // position is y=0, which isn't represented in moduleBottoms.
      var nearestDist = Math.abs(movingY)
      var nearestIdx = 0  // New index of moving module, moduleBottoms.length to insert at end
      for(i=0; i<moduleBottoms.length; ++i) {
        var dist = Math.abs(moduleBottoms[i].y - movingY)
        if(dist < nearestDist) {
          nearestIdx = i+1
          nearestDist = dist
        }
      }

      // If the module isn't changing position, there's nothing to do (avoid
      // calling Client.applySettings() excessively)
      if(nearestIdx === currentMovingIdx)
        return

      // Insert the module in the new position
      var newModules = moduleBottoms.map(function(mod){return mod.key})
      newModules.splice(nearestIdx, 0, movingModuleKey)
      applyModuleList(newModules)
    }
  }

  // All of the module objects.  The order here doesn't matter.
  SeparatorModule {
    id: separatorModule
    moduleData: moduleData
  }
  QuickConnectModule {
    moduleData: moduleData
  }
  UsageModule {
    moduleData: moduleData
  }
  PerformanceModule {
    moduleData: moduleData
  }
  ConnectionModule {
    moduleData: moduleData
  }
  SettingsModule {
    moduleData: moduleData
  }
  SnoozeModule {
    moduleData: moduleData
  }

  AccountModule {
    moduleData: moduleData
  }
  RegionModule {
    moduleData: moduleData
  }
  IPModule {
    moduleData: moduleData
  }

  // Sanitize a list of modules - ensure all modules are present, remove unknown
  // modules, and remove duplicates.  Returns the sanitized list.
  function sanitizeModuleList(moduleList) {
    // Add missing modules at the bottom, below the fold.
    //
    // This happens when dragging a module from above the separator to below.
    // We briefly observe a transient where the module is not in either list,
    // since the primaryModules setting is notified before the secondaryModules
    // setting.
    //
    // Adding the module somewhere prevents a transient change in the overall
    // ModuleSorter height.  It doesn't really matter where (the transient will
    // be resolved before the next frame is rendered, since we immediately
    // observe the corresponding change in secondaryModules).  We just have to
    // prevent the height change, since in windowed mode this causes a flicker.
    //
    // This also would happen if we add new modules in the future; they will be
    // added at the bottom below the fold.
    var fixedModuleList = moduleList.slice()
    for(var moduleKey in modules) {
      if(fixedModuleList.indexOf(moduleKey) === -1) {
        console.info('Added missing module ' + moduleKey)
        fixedModuleList.push(moduleKey)
      }
    }
    // Remove any modules that do not exist, and remove duplicate modules.
    // Duplicates happen transiently when dragging from below the separator to
    // above, just like the case above, since we observe the primaryModules
    // change first.
    var observedModules = {}
    var i=0
    var modKey
    while(i<fixedModuleList.length) {
      modKey = fixedModuleList[i]
      if(!modules[modKey]) {
        fixedModuleList.splice(i, 1)
        console.info('Removed unknown module ' + modKey)
      }
      else if(observedModules[modKey]) {
        fixedModuleList.splice(i, 1)
        console.info('Removed duplicate module ' + modKey + ' (' + JSON.stringify(observedModules) + ')')
      }
      else {
        observedModules[modKey] = true
        ++i
      }
    }

    return fixedModuleList
  }

  // Update the modules' order in the children list to match the module list.
  function restackModules() {
    var priorModule, thisModule
    for(var i=0; i<moduleList.length; ++i) {
      thisModule = modules[moduleList[i]]
      if(thisModule) {
        if(priorModule) {
          NativeHelpers.itemStackAfter(priorModule, thisModule)
        }
        priorModule = thisModule
      }
    }
  }

  Component.onCompleted: {
    // The modules' order affects tab and screen reader navigation.  Keep the
    // modules in the proper order in the children list.
    restackModules()
    onModuleListChanged.connect(restackModules)
  }
}
