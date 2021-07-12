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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.10
import PIA.NativeHelpers 1.0
import '../theme'
import '../common'
import '../core'
import '../client'

SecondaryWindow {
  id: onboardingWindow
  contentLogicalWidth: Theme.onboarding.width
  contentLogicalHeight: Theme.onboarding.height
  title: uiTr("Quick Tour")

  function showOnboarding () {
    // Reset to the first page if the window isn't already open
    if(!onboardingWindow.visible)
      Client.uiState.onboarding.currentPage = 0
    onboardingWindow.open();
  }

  function closeAndShowDashboard () {
    onboardingWindow.close();
    dashboard.window.showDashboard(trayManager.getIconMetrics())
  }

  visible: false

  Loader {
    id: pageControllerLoader
    width: contentLogicalWidth
    height: contentLogicalHeight
    // We don't need to apply onboardingWindow.positioningForShow here,
    // because this window's size doesn't depend on the content.
    active: onboardingWindow.visible
    sourceComponent: Component { PageController {} }
  }
}
