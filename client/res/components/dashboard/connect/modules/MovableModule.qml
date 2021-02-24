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
import QtQuick.Window 2.11
import "../../../client"
import "../../../common"
import "../../../core"
import "../../../theme"
import PIA.DragHandle 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil

// MovableModule provides the common elements displayed by movable modules - the
// bookmark button and top line.  (SeparatorModule is the only module that isn't
// movable.)
Module {
  id: module

  // Localized name of the tile.  Used for screen reader annotations on the
  // bookmark button.
  property string tileName

  default property alias childrenContainerData: childrenContainer.data

  // Whether to show the bookmark icon (safe alias for moduleData.showBookmarks)
  readonly property bool showBookmark: moduleData ? moduleData.showBookmarks : false

  // Switch the list that this module is in (used to implement bookmark button)
  function switchModuleList() {
    var primaries = Client.settings.primaryModules
    var secondaries = Client.settings.secondaryModules
    if(primaries.indexOf(moduleKey) !== -1) {
      primaries = primaries.filter(function(val){return val !== moduleKey})
      secondaries.push(moduleKey)
    }
    else {
      secondaries = secondaries.filter(function(val){return val !== moduleKey})
      primaries.push(moduleKey)
    }
    Client.applySettings({primaryModules: primaries, secondaryModules: secondaries})
  }

  // Move the module in the module list (used to implement key navigation for
  // drag grabber / bookmark button)
  function moveModule(direction) {
    var modList = moduleData.moduleList.slice()
    var moduleIndex = modList.indexOf(moduleKey)
    if(moduleIndex < 0)
      return  // Shouldn't happen, module is missing somehow
    modList.splice(moduleIndex, 1)  // Delete this module
    moduleIndex += direction
    moduleIndex = Math.max(moduleIndex, 0)
    moduleIndex = Math.min(moduleIndex, modList.length)
    modList.splice(moduleIndex, 0, moduleKey) // Insert in the new location
    // Apply the new module list
    moduleData.applyModuleList(modList)
  }

  // In the 'dragging' state, the current drag position of the drag handle, in
  // the drag handle's coordinates
  property point handleDragPos
  // In the 'dragging' state, the current desired screen position of the drag
  // point
  property point handleDragDesiredPos

  // Use a signal to call this function from the Y property binding, so it
  // doesn't add spurious dependencies to the Y binding.
  signal checkModuleOrder()
  onCheckModuleOrder: module.moduleData.checkModuleOrder(moduleKey)

  // MovableModule uses states to switch between the normal positioning bindings
  // and the bindings used during a drag.
  state: 'idle'
  states: [
    // In the 'idle' state, the module is not being dragged.  (It might be
    // animating due to a bookmark click though.)  X and Y are bound to the
    // regular computed positions, and Y animations are enabled.
    State {
      name: 'idle'
      PropertyChanges {
        target: module
        x: 0
        y: computedY
        // When a Y animating, stack above non-animating modules.  This keeps
        // a module on top after a drag as it settles to its fixed position.
        z: yAnimationRunning ? 1 : 0
        // When a module is being moved with the bookmark button, don't animate
        // its Y position.  (The module content isn't visible when the Y
        // coordinate changes, but the top line still is.)
        yAnimationEnabled: !bookmarkSlideSequence.running
      }
      StateChangeScript {
        // Fade the drag indicator only when ending a drag; it shows instantly
        // when starting a drag.  (Initiated with a script instead of a
        // transition so it doesn't hold up the state transition; we want the
        // normal Y behavior to occur at the same time.)
        script: dragFadeOutAnimation.start()
      }
    },
    // In the 'dragging' state, the module is being dragged.
    State {
      name: 'dragging'
      PropertyChanges {
        target: module
        // Offset X so the module appears to pop out of the dashboard.  The full
        // visual effect only works when there's padding around the dashboard,
        // in windowed/masked modes the module is still clipped to the dashboard
        // bound.
        x: Theme.dashboard.moduleDragOffset
        // Compute Y from the drag position
        y: {
          var modParent = module.parent
          // This computation depends on the module's parent's absolute position
          // (but not the module's absolute position, that'd be a binding loop)
          Util.dependAbsolutePosition(modParent, modParent.Window.window)
          // Map the desired position to the module's parent's coordinates
          var dragParentPos = modParent.mapFromGlobal(handleDragDesiredPos.x, handleDragDesiredPos.y)
          // The top Y position is that point's Y minus the drag position.  (No
          // transformations are applied to the module.)
          var y = dragParentPos.y - handleDragPos.y

          // When Y is recomputed during the dragging state, tell the module
          // sorter to check the ordering of the modules.  The module order
          // doesn't affect this module's Y in this state.
          //
          // (Bindings with side effects are generally frowned upon, but the
          // alternative is just making on onYChanged connection that has to
          // check the state, which effectively just figuring out when this
          // computation occurred.  Note that this is _not_ equivalent to
          // calling this in onDragUpdatePosition(), because the item position
          // is also recomputed if the window moves to stay with the cursor,
          // which could cause another reordering.)
          module.checkModuleOrder()

          // Limit the module to the edges of the parent (it'd be obscured by
          // the header / expand button otherwise)
          y = Math.max(y, 0)
          y = Math.min(y, modParent.height - module.height)
          return y
        }
        // Stack above non-dragging modules
        z: 2
        // No Y animations
        yAnimationEnabled: false
      }
      StateChangeScript {
        // Stop the fade out animation if it's running, show the drag indicator
        // immediately when starting a drag
        script: {
          dragFadeOutAnimation.stop()
          dragIndicator.opacity = 1
        }
      }
    }
  ]

  Rectangle {
    id: dragIndicator
    anchors.fill: parent
    color: Theme.dashboard.moduleDragOverlayColor
    NumberAnimation {
      id: dragFadeOutAnimation
      target: dragIndicator
      property: "opacity"
      duration: Theme.animation.normalDuration
      to: 0
    }
  }

  // Wrapper for horizontal slides from bookmark buttons
  Item {
    id: slideWrapper
    y: 0
    x: 0
    width: parent.width
    height: parent.height

    // Animation used to execute a bookmark / unbookmark action
    SequentialAnimation {
      id: bookmarkSlideSequence
      // Slide out of view
      NumberAnimation {
        target: slideWrapper
        property: "x"
        duration: Theme.animation.quickDuration
        from: 0
        to: parent.width
      }
      // Move the module to the other section
      ScriptAction {
        script: {
          var oldY = module.computedY
          module.switchModuleList(module.moduleKey)
          var newY = module.computedY

          // If the module is moving downward, slide the separator line from the
          // bottom of the module up while the modules move around.  Otherwise,
          // the separator line would suddenly appear in the middle of the
          // preceding module.
          if(newY > oldY)
            topLine.y = module.height-topLine.height
          // Otherwise, the module is moving upward, and the line should stay at
          // the top.
        }
      }
      // Animate the top line back as the modules move around.  (Still needed
      // even if the top line isn't moving; in that case it's just a pause while
      // the other modules move.)
      NumberAnimation {
        target: topLine
        property: "y"
        duration: Theme.animation.quickDuration
        to: 0
      }
      // Slide back into view
      NumberAnimation {
        target: slideWrapper
        property: "x"
        duration: Theme.animation.quickDuration
        from: parent.width
        to: 0
      }
    }

    // The module contents are inside the slide wrapper but stack below the
    // bookmark button.
    Item {
      id: childrenContainer
      anchors.fill: parent
    }

    Item {
      id: bookmarkContainer
      anchors.top: parent.top
      anchors.right: parent.right
      anchors.topMargin: 4
      // Keep the bookmark icon itself out of the scroll bar region - this lines
      // it up with the edge of the scroll bar so the whole bookmark image is
      // interactive.  (Some of the invisible mouse area is still covered by the
      // scroll bar.)
      anchors.rightMargin: 3
      height: 24
      width: 40

      opacity: module.showBookmark ? 1 : 0
      Behavior on opacity {
        NumberAnimation {
          duration: Theme.animation.normalDuration
        }
      }
      visible: opacity > 0

      // The drag handle and bookmark button act as one focusable item.  Having
      // two tabstops here would create a lot of tabstops between modules that
      // really aren't all that useful.
      //
      // The drag handle can't really be expressed to screen readers that well
      // on its own either, it doesn't really have a "press" action.  Grouping
      // them means that the bookmark button becomes the "press" action and the
      // up/down movement can be alternate actions, which is pretty reasonable.
      activeFocusOnTab: true

      //: Screen reader annotation for the "bookmark" button on tiles.  This
      //: behaves like a checkbox that can be toggled, i.e.
      //: "this is a favorite tile" - set to on/off.
      NativeAcc.MoveButton.name: uiTranslate("ModuleLoader", "Favorite tile")
      NativeAcc.MoveButton.description: {
        if(module.isAboveFold) {
          //: Screen reader annotation for 'active' tile bookmark button that
          //: will remove a tile from favorites.  %1 is a tile name, like
          //: "Performance tile", "Account tile", etc.
          return uiTranslate("ModuleLoader", "Remove %1 from favorites").arg(module.tileName)
        }
        //: Screen reader annotation for 'inactive' tile bookmark button that
        //: will add a tile to favorites.  %1 is a tile name, like
        //: "Performance tile", "Account tile", etc.
        return uiTranslate("ModuleLoader", "Add %1 to favorites").arg(module.tileName)
      }
      NativeAcc.MoveButton.onActivated: toggleBookmark()
      NativeAcc.MoveButton.onMoveUp: module.moveModule(-1)
      NativeAcc.MoveButton.onMoveDown: module.moveModule(1)

      function toggleBookmark() {
        // Ignore clicks while dragging (difficult to do but not impossible)
        if(module.state === 'idle')
          bookmarkSlideSequence.start()
      }

      DragHandle {
        height: parent.height
        width: parent.width/2
        onDragUpdatePosition: {
          // Go to the 'dragging' state if we're not already dragging and the
          // bookmark animation isn't running
          if(module.state === 'idle' && !bookmarkSlideSequence.running) {
            module.state = 'dragging'
          }
          // If we're dragging (either just now or from a prior event), set the
          // drag positions
          if(module.state === 'dragging') {
            handleDragPos = dragPos
            handleDragDesiredPos = desiredScreenPos
          }
        }

        onDragEnded: {
          if(module.state === 'dragging')
            module.state = 'idle'
        }
      }

      MouseArea {
        x: parent.width/2
        height: parent.height
        width: parent.width/2

        enabled: module.showBookmark
        // enabled=false doesn't prevent changing the cursor, hide the item for
        // that.
        visible: module.showBookmark
        cursorShape: Qt.PointingHandCursor
        onClicked: bookmarkContainer.toggleBookmark()
      }

      readonly property real imgCenterSpacing: 20

      Image {
        x: parent.width/2 - parent.imgCenterSpacing/2 - width/2
        y: parent.height/2 - height/2
        height: sourceSize.height/2
        width: sourceSize.width/2
        source: module.isAboveFold ? Theme.dashboard.moduleGrabberFavoriteImage : Theme.dashboard.moduleGrabberImage
      }

      Image {
        x: parent.width/2 + parent.imgCenterSpacing/2 - width/2
        y: parent.height/2 - height/2
        height: sourceSize.height/2
        width: sourceSize.width/2
        source: module.isAboveFold ? Theme.dashboard.moduleBookmarkOnImage : Theme.dashboard.moduleBookmarkOffImage
      }

      OutlineFocusCue {
        id: focusCue
        anchors.fill: parent
        control: parent
        borderMargin: 0
      }

      Keys.onPressed: {
        if(KeyUtil.handleButtonKeyEvent(event)) {
          focusCue.reveal()
          toggleBookmark()
        }
        else if(event.key === Qt.Key_Up) {
          event.accepted = true
          focusCue.reveal()
          module.moveModule(-1)
        }
        else if(event.key === Qt.Key_Down) {
          event.accepted = true
          focusCue.reveal()
          module.moveModule(1)
        }
      }
    }
  }

  // Top line - does not slide horizontally with module
  Rectangle {
    id: topLine
    x: 0
    y: 0
    width: parent.width
    height: Theme.dashboard.moduleBorderPx
    color: Theme.dashboard.moduleBorderColor
  }
}
