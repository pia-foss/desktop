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
// ClientUIState contains state controlled by the UI QML code.  This is state
// that would otherwise be held in local properties, but is instead stored here
// to preserve it when the client UI is destroyed and recreated.
//
// This currently happens when the dashboard frame behavior is changed
// (popup/windowed), in the future it might also allow us to destroy the client
// UI when it hasn't been used for a while, etc.
QtObject {
  // Dashboard-related UI state
  readonly property var dashboard: QtObject {
    // Whether the dashboard is currently shown - used by DashboardWindow and
    // DashboardPopup.
    // - Allows the dashboard to re-show itself if the frame is changed while
    //   it's shown
    // - Allows the dashboard to show itself if the splash dashboard was shown
    //   before the dashboard loaded (even if the app was launched in quiet
    //   mode)
    property bool shown: false

    // Scroll positions for dashboard pages; held here since the pages are
    // unloaded when not visible.
    property real connectScrollPos: 0
    property real regionsScrollPos: 0
  }

  readonly property var settings: QtObject {
    property bool gatheringDiagnostics: false
    property int currentPage: 0
    property var knownCustomServers: null
  }

  readonly property var onboarding: QtObject {
    property int currentPage: 0
  }
}
