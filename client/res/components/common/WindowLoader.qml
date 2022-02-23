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

import QtQuick 2.9
import PIA.NativeHelpers 1.0

// Wrapper to dynamically load a DecoratedWindow the first time it is shown.
// Provides an open() method that either loads and displays the window, or
// re-displays the window if it's been shown before.
//
// The window is shown with its open() method (DecoratedWindow provides such a
// method for most PIA windows).
//
// Currently there's no way to unload a window, this is currently only used for
// Dev Tools, which is not normally used by most users (and adds a significant
// amount of time to the initial load due to TextArea layout processing).
QtObject {
  id: windowLoader

  // Note: Set QtObject.objectName to specify the name used when tracing for
  // this object

  property alias component: loader.sourceComponent

  // The Loader object's id is 'loader', but since this is a QtObject and not an
  // Item, we have to explicitly name a property for it to go in ('loaderProp')
  property Loader loaderProp: Loader {
    id: loader
    active: false // Initially not active, loaded when needed
    onLoaded: {
      loader.item.open()
    }
  }

  // The loaded window, once it has been loaded by open()
  property alias window: loader.item

  function open() {
    switch(loader.status) {
      case Loader.Null:
        // First time being shown, activate to load the component.  onLoaded()
        // will show after the load completes
        console.info('Loading ' + objectName)
        loader.active = true
        break
      case Loader.Ready:
        console.info('Already loaded ' + objectName + ' - show again')
        loader.item.open()
        break
      case Loader.Loading:
        // Nothing to do, already loading it, it'll be shown by onLoaded()
        console.info('Currently loading ' + objectName + ' - ignore additional request')
        break
      case Loader.Error:
        console.warn('Error occurred loading ' + objectName + ' - can\'t show it')
        break
    }
  }
}
