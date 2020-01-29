// Copyright (c) 2020 Private Internet Access, Inc.
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

SecondaryWindow {
  id: onboardingWindow
  contentLogicalWidth: Theme.onboarding.width
  contentLogicalHeight: Theme.onboarding.height
  title: uiTr("Quick Tour")

  function showOnboarding () {
    onboardingWindow.open();
  }

  function closeAndShowDashboard () {
    onboardingWindow.close();
    dashboard.window.showDashboard(trayManager.getIconMetrics())
  }

  visible: false

  PageController {
      id: pageController
      width: contentLogicalWidth
      height: contentLogicalHeight
  }
}
