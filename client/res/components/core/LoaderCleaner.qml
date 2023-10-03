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

pragma Singleton
import QtQml 2.15 as QtQml

QtQml.QtObject {
  id: loaderCleaner

  // Signal emitted to actually trigger a cleanup - this is handled in
  // main.qml, which is aware of all the windows in the program (some cleanup
  // is applied to each window individually).
  signal cleanupTriggered()

  // Whenever a cleanup occurs, we clean up twice, in case the first cleanup
  // causes more references to be unneeded but not cleaned up immediately
  // (which is particularly relevant since much of Qt's cleanup uses
  // deleteLater().)
  readonly property int initialCleanupTime: 3000  // 3 seconds
  readonly property int finalCleanupTime: 6000 // 6 seconds (after initial cleanup)
  property var cleanupTimer: QtQml.Timer {
    interval: initialCleanupTime
    repeat: false
    running: false
    onTriggered: {
      // We shouldn't deactivate any loaders when doing cleanup, but if it
      // does occur, we'll still properly handle first/second cleanups by
      // checking the initial state.  (Don't reset back to the first cleanup if
      // an unload occurs here, or we might get stuck in a cleanup loop if it
      // happened.)
      let wasFirstCleanup = cleanupTimer.interval === loaderCleaner.initialCleanupTime
      loaderCleaner.cleanupTriggered()
      if(wasFirstCleanup) {
        console.info("Clean up again in " + loaderCleaner.finalCleanupTime + " ms")
        cleanupTimer.interval = loaderCleaner.finalCleanupTime
        cleanupTimer.restart()
      }
      else {
        console.info("Second clean up completed")
      }
    }
  }

  function loaderDeactivated() {
    // Do the first cleanup after no loaders have deactivated for 30 seconds.
    // (If we had done the first cleanup and were waiting to do the second,
    // reset back to the short interval.)
    cleanupTimer.interval = loaderCleaner.initialCleanupTime
    cleanupTimer.restart()
  }
}
