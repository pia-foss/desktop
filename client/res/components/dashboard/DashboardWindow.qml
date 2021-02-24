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
import QtQuick.Controls 2.4
import QtQuick.Window 2.10

import PIA.WindowClipper 1.0
import PIA.WindowMaxSize 1.0
import PIA.NativeHelpers 1.0
import PIA.BrandHelper 1.0
import PIA.WindowScaler 1.0
import "../theme"
import "../common"
import "../client"

// DashboardWindow is a DecoratedWindow, but not a SecondaryWindow.
// Unlike the various modeless dialogs that SecondaryWindow is used for,
// DashboardWindow handles its own maximum size and scrolling.  It doesn't use
// a simple "scroll the whole content" model like SecondaryWindow; instead the
// dashboard content handles scrolling on a per-page basis.
DecoratedWindow {
  id: dashWindow

  // The dashboard doesn't have any provision for scrolling horizontally.  This
  // just assumes that the display has a logical width of at least 300px (plus
  // window decoration / taskbar).
  windowLogicalWidth: Theme.dashboard.width
  // The desired client size for the windowed dashboard is just the fixed
  // width and the height expressed by the content.
  // WindowMaxSize checks the work area size, scale, window decoration, etc.,
  // and gives us a smaller size as maxSize if necessary.
  windowLogicalHeight: content.pageHeight

  // Properties common to DashboardWindow and DashboardPopup
  readonly property bool slideBlending: false // Used in DashboardWrapper

  // Even with the color set this way, we still get a brief flash of white when
  // expanding the windowed-mode dashboard on Windows.  Qt probably sets a white
  // background brush in the window class (which is super-Windows-specific and
  // not configurable); we might want to go behind its back and change the
  // background brush to the dashboard color (which would probably be OK since
  // all windows in this app use this color).
  color: Theme.dashboard.backgroundColor
  title: BrandHelper.brandName
  visible: false

  // Like DashboardPopup, the 'content' properties should be set based on the
  // content of the dashboard.
  property DashboardContentInterface content: DashboardContentInterface {}

  function trayClicked() {
    // Show and activate the dashboard.
    // The windowed dashboard can be visible but not active, in which case we
    // want to activate it.
    //
    // Also, note that DashboardPopup reactivates the settings window too on
    // Mac if it was visible.  We don't do this in DashboardWindow; it's not
    // supported on Mac, and for whatever reason still gets the settings window
    // stuck in an unfocused state (probably because DashboardPopup defers its
    // activation for a frame, but DashboardWindow does not)
    showDashboard()
  }

  function showDashboard() {
    // The tray/screen metrics are ignored.  The windowed dashboard instead uses
    // DecoratedWindow's placement behavior (center on the current/primary
    // screen), and size/scale based on the current screen.
    //
    // (Historically, it used to try to use the same placement as DashboardPopup
    // to make toggling between the two somewhat seamless.  This works
    // differently now, but it'd still be possible in principle by using a
    // DashboardPositioner to determine where the popup would be placed with the
    // metrics given, and move to that position.)
    open()
  }

  function showFromTrayMenu() {
    // We never need to defer showing the windowed dashboard; even if the tray
    // takes focus back on Linux, we won't hide the windowed dashboard.
    showDashboard()
  }

  function focusIfVisible() {
    if(visible)
      open()
  }

  readonly property int contentRadius: 0
  readonly property int layoutHeight: actualLogicalHeight
  // For the windowed dashboard, notifications are suppressed only if it's
  // focused, not just when it's visible.  It's frequently visible but obscured
  // by other windows, and there's no great way to figure that out.
  readonly property bool suppressNotifications: active

  // Handle close keys.
  // Alt+F4 on Windows can be handled by the window procedure, but Cmd+W on Mac
  // is our responsibility.
  Shortcut {
    sequence: StandardKey.Close
    context: Qt.WindowShortcut
    onActivated: hide()
  }

  // When the dashboard is shown/hidden, update UIState
  onVisibleChanged: Client.uiState.dashboard.shown = dashWindow.visible

  Component.onCompleted: {
    NativeHelpers.initDashboardPopup(dashWindow)

    // Like DashboardPopup, show if a prior dashboard was visible, and update
    // UIState
    if(Client.uiState.dashboard.shown)
      showDashboard()
  }
}
