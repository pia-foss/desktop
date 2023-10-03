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

import QtQuick 2.9
import QtQuick.Controls 2.3
import "../client"

// PersistentFlickable is a Flickable that stores and reloads its scroll
// position from a property in ClientUIState.  This allows the Flickable to be
// seamlessly unloaded and re-loaded; its scroll position is preserved as if it
// had remained loaded.
Flickable {
  id: persistentFlickable
  // Configure the group and property name within ClientUIState where the scroll
  // position is stored.  For example, to use
  // Client.uiState.dashboard.regionsScrollPos, set these to "dashboard" and
  // "regionsScrollPos".
  //
  // If either is not set, PersistentFlickable does not store its scroll
  // position.
  property string stateGroupName
  property string statePropertyName

  onContentYChanged: {
    if(persistentFlickable.stateGroupName &&
      persistentFlickable.statePropertyName)
      Client.uiState[stateGroupName][statePropertyName] = persistentFlickable.contentY
  }
  Component.onCompleted: {
    if(persistentFlickable.stateGroupName &&
      persistentFlickable.statePropertyName)
      persistentFlickable.contentY = Client.uiState[stateGroupName][statePropertyName]
  }
}
