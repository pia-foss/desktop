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
import QtQuick.Controls 2.4
import QtQuick.Window 2.10
import QtGraphicalEffects 1.0

import PIA.WindowClipper 1.0
import PIA.WindowFormat 1.0
import PIA.WindowScaler 1.0
import PIA.WorkspaceChange 1.0
import PIA.NativeHelpers 1.0
import "../theme"
import "../client"
import "../common"
import "../core"
import "qrc:/javascript/util.js" as Util

PiaWindow {
  id: dashWindow
  // The window's position is in screen coordinates, so scale dashPlacement's
  // logical coordinates
  x: dashPlacement.frameX * dashPlacement.screenScale
  y: dashPlacement.frameY * dashPlacement.screenScale + workspaceYHack

  // Whether to shape the dashboard using alpha blending or a window mask.
  //
  // The preferred mode is to alpha blend the dashboard.  The rendering is much
  // better (antialiased edges, drop shadow), and the window bound is properly
  // synchronized with the window content.
  //
  // Shaping using a window mask is a fallback.  This eliminates alpha blending
  // and creates a rounded window mask to shape the dashboard.  This doesn't
  // render quite as perfectly (jagged edges, no drop shadow), and it causes the
  // expansion animation to be somewhat unsynchronized with the dashboard
  // content.  However, it works more reliably.
  readonly property bool shapeWithMask: {
    // On Linux, we have no way to do alpha blending at all.  (It might be
    // possible with the COMPOSITE extension and appropriate support from the
    // compositing manager, but Qt doesn't support that even if it is.)
    //
    // The window mask is the only possible way to shape the window.  It relies
    // on the XSHAPE extension (which is ubiquitous; that's how xeyes/xclock
    // shape their windows).  On the off chance this does not work, the
    // dashboard is still pretty usable, it just has a gray bound that extends
    // to the top of the work area.
    if(Qt.platform.os === 'linux')
      return true

    // On Windows, alpha blending is preferred, but some systems have issues
    // with it.  We're not sure exactly what causes it, but on some systems,
    // enabling transparency on the taskbar causes the dashboard to get some
    // sort of additive blending.  It might be an OS bug, or it might be a
    // driver failing to reset some state between contexts, etc.
    if(Qt.platform.os === 'windows' && Client.state.usingSafeGraphics)
      return true

    return false
  }

  // When using alpha blending, make the dashboard window transparent - this
  // causes QQuickWindow to alpha blend the window.
  //
  // For shaping with a window mask, paint with the background color instead
  // both to disable alpha blending and ensure that any visible part of the
  // window has the proper background color.
  //
  // On Windows, setting an opaque color removes the WS_EX_LAYERED style, which
  // should work around some strange additive blending issues that some users
  // have reported.  (We also have to ensure the window format doesn't have an
  // alpha channel; see WindowFormat.hasAlpha below.)
  color: shapeWithMask ? Theme.dashboard.backgroundColor : Qt.rgba(0, 0, 0, 0)

  //: Title of the dashboard window (the main UI that users interact with.
  //: This isn't normally shown, but it is used by screen readers, and it is
  //: occasionally used by tools on Windows that list open application windows.
  //: "PIA" stands for Private Internet Access.  We refer to this window as the
  //: "dashboard", but this term doesn't currently appear elsewhere in the
  //: product.
  title: uiTr("PIA Dashboard")
  // The shadow provided by default on OS X doesn't animate properly when the
  // dashboard expands/collapses.  Disable it and use a pre-rendered shadow to
  // provide the shadow when in popup mode.
  flags: {
    var flags = Qt.FramelessWindowHint|Qt.NoDropShadowWindowHint
    // On Windows, the icon by default will be in the tray overflow area, which
    // would cover part of the dashboard as it's always-on-top.  Give the dash
    // the on-top flag so we cover it instead.
    // We don't want this on Mac, because by default on small Macbooks the
    // settings window would overlap the dashboard, and this prevents it from
    // covering the dash.  (It still prevents it from covering the dash on
    // Windows, but the tray overflow is a bigger concern there.)
    if(Qt.platform.os === 'windows')
      flags |= Qt.WindowStaysOnTopHint
    return flags
  }

  // Children of DashboardPopup are displayed in its contentItem.  The content
  // should respect layoutPageHeight, and the DashboardPopup's pageHeight,
  // maxPageHeight, and header opacities should be set based on the content.
  default property alias contents: contentItem.children

  property DashboardContentInterface content: DashboardContentInterface {}

  // The settings window (if it exists) - this is also raised when the tray is
  // clicked on Mac.  It has to be handled by DashboardPopup because the
  // activation sequence is extremely touchy.
  property Window settingsWindow

  // Set the window's format to include or not include an alpha channel.  This
  // has to be specified this way to ensure that it's set before the
  // WindowScaler and friends cause the underlying platform window to be created
  // (which they cannot avoid doing because there's no way to detect whether the
  // platform window has been created yet).
  //
  // This is necessary on Windows to ensure we don't apply the WS_EX_LAYERED
  // style - otherwise, Qt would still apply it even with a solid background
  // color, because the window would have the FramelessWindowHint flag and an
  // alpha channel.
  WindowFormat.hasAlpha: !shapeWithMask

  WindowClipper {
    targetWindow: dashWindow
    // Window coordinates - scale dashPlacement's logical coordinates
    clip: {
      // Use the clip mask computed by dashPlacement, and scale to screen
      // coordinates.
      return Qt.rect(dashPlacement.clip.x * dashPlacement.screenScale,
                     dashPlacement.clip.y * dashPlacement.screenScale,
                     dashPlacement.clip.width * dashPlacement.screenScale,
                     dashPlacement.clip.height * dashPlacement.screenScale)
    }
    // Round the clip region when shaping with the mask.  This isn't necessary
    // when alpha blending (and probably has a performance cost).
    round: shapeWithMask ? Theme.dashboard.windowRadius : 0
  }

  WindowScaler {
    id: scaler
    targetWindow: dashWindow
    // logicalSize is set using the change bindings below.

    // The popup dashboard doesn't have a window caption, but we still get this
    // event if the user presses Alt+F4.
    onCloseClicked: dashWindow.animHide()
  }

  // This hack is needed to work around spurious binding loops.
  //
  // When a notification appears or changes, this can cause the dashboard's
  // height to change.  If it does, the dashboard is resized (usually - this
  // depends on platform-specific logic controlling whether we resize or clip
  // the dashboard).
  //
  // Although resizing the dashboard doesn't actually affect the layout (the
  // scale wrapper's size isn't affected by the actual dashboard size), Qt
  // recalculates positioners and layouts anyway.  On Mac OS, this appears to
  // occur because it interprets the resize as an "expose" event, and it seems
  // to try to defer hidden positioning until the dashboard is "exposed".
  //
  // When this happens, it seems to cause implicit size changes even though
  // the item sizes did not actually change (breaking on
  // QQmlAbstractBinding::printBindingLoopError shows that this is occurring,
  // although without private Qt symbols it is tough to tell what sizes/objects
  // are affected).  That ends up triggering a binding loop because an implicit
  // height change triggered the layout in the first place.
  //
  // This might only happen on Mac OS because window changes are applied
  // synchronously by the window manager - on Windows and Linux, the window
  // managers are asynchronous.
  //
  // Deferring the window resize breaks the binding loop (or at any rate,
  // prevents Qt from detecting it).  It won't loop infinitely, because the
  // logical size does not change the second time the layout occurs.
  readonly property size logicalSize: Qt.size(dashPlacement.frameWidth, dashPlacement.frameHeight)
  function applyLogicalSize() {
    scaler.logicalSize = logicalSize
  }
  onLogicalSizeChanged: sizeHackTimer.start()
  Timer {
    id: sizeHackTimer
    repeat: false
    interval: 1
    onTriggered: applyLogicalSize()
    Component.onCompleted: applyLogicalSize()
  }
  // Qt does not compute layouts while the window is hidden.  We have to
  // initially show the window with 0 opacity so Qt will compute layouts (which
  // allows us to determine the window size), then actually render the window
  // later.  (Otherwise, the window shows up cropped initially, then a frame
  // later it resizes correctly.)
  //
  // This is not strictly due to the size-deferring hack above, it's also
  // necessary because notifications could appear/disappear while the window is
  // hidden, and we the new height won't be known until the window is shown
  // since Qt does not calculate layouts while the window is hidden.
  property bool calculatingLayouts: true

  // This hack is needed to work around a Qt issue on Mac OS.  If the work area
  // changes size, the dashboard's Y coordinate usually does not change (since
  // the menu bar is at the top of the screen).  However, the underlying Mac OS
  // coordinates are reversed (origin at the bottom, not top), so the actual
  // underlying coordinate _should_ change.
  //
  // Qt, however, does not pay attention to this, so it doesn't reapply the
  // Y-coordinate.  Do it here by hacking the Y-coordinate by 1 pixel for 1
  // frame.
  property int workspaceYHack: 0
  Timer {
    id: workAreaHackTimer
    repeat: false
    interval: 1
    onTriggered: workspaceYHack = 0
  }
  WorkspaceChange {
    onWorkspaceChanged: {
      // Hide the dashboard if it is shown - it probably isn't in the right
      // place any more, and we only know the new tray location when the icon is
      // selected.
      animHide()
      if(Qt.platform.os === 'osx') {
        console.info('Workspace changed, reapply dashboard Y position')
        workspaceYHack = 1
        workAreaHackTimer.start()
      }
    }
  }

  // This 'contentScale' property is used by various elements of the dashboard
  // that need to scale based on the screen DPI.  All top-level windows
  // (DashboardPopup, DashboardWindow, and SettingsWindow) provide this property.
  readonly property real contentScale: scaler.scale
  // Additional margin applied by popups in the dashboard (to compensate for the
  // shadow padding in DashboardPopup).  Overlays must respect this value,
  // otherwise they'll be clipped off when shaping with a window mask.
  // All top-level windows provide this property too (it is only nonzero for
  // DashboardPopup).
  readonly property real popupAddMargin: dashPlacement.windowPadding
  // Whether the dashboard is alpha-blending due to a slide animation
  // (DashboardWrapper uses this)
  readonly property bool slideBlending: slideAnimator.dashOpacity < 1

  DashboardSlideAnimator {
    id: slideAnimator
  }

  function animShow() {
    slideAnimator.dashVisibleState = true
  }

  function animHide() {
    slideAnimator.dashVisibleState = false
  }

  visibility: slideAnimator.dashVisible ? Window.Windowed : Window.Hidden

  DashboardPopupPlacement {
    id: dashPlacement
    pageHeight: content.pageHeight
    dashWidth: Theme.dashboard.width
    shapeWithMask: dashWindow.shapeWithMask
    // Animate the mask when shaping the window this way.  Not synchronized with
    // content, but still looks pretty good, and better than the alternative of
    // snapping the mask to the larger size.
    dashAnimatingHeight: wrapperHeightAnimation.running ? contentItem.height : 0
    slideOffset: slideAnimator.dashSlideOffset
    windowPadding: dashWindow.windowPadding
  }

  // Overlay scale and RTL flip - see SecondaryWindow
  Overlay.overlay.transform: [
    Scale {
      origin.x: scaleWrapper.width/2
      xScale: dashWindow.rtlFlip
    },
    Scale {
      xScale: contentScale
      yScale: contentScale
    }
  ]

  // Apply a scale transformation to the content of the window, so it's laid out
  // in logical coordinates.
  Item {
    id: scaleWrapper
    // The scale wrapper is sized and positioned based on the maximum dashboard
    // size.  The scale wrapper cannot move when the dashboard window is clipped
    // on OS X; this causes the content of the dashboard to jump around for 1
    // frame until everything syncs up again.
    x: 0
    y: 0
    width: dashPlacement.frameWidth
    height: dashPlacement.dashActualMaxHeight
    // RTL flip and scale
    transform: [
      Scale {
        origin.x: scaleWrapper.width/2
        xScale: dashWindow.rtlFlip
      },
      Scale {
        xScale: contentScale
        yScale: contentScale
      }
    ]

    // All of the visible contents of the window are wrapped in slideWrapper to
    // slide them in when the dashboard appears/disappears.
    //
    // Generally, it's best just not to animate the top-level window's position,
    // this isn't generally synchronized with other animations since most window
    // managers are asynchronous.  Some types of movements can also cause
    // artifacts.
    //
    // For the slide animation specifically, there are a few reasons that we need
    // this window:
    // - On OS X, we can't position the outer window outside of the work area
    //   bounds, so we have to slide in the inner window.
    // - The slide animation can't be applied to DashboardWrapper, it would
    //   interfere with its other animations.
    // - We want to crop the window to avoid showing on top of the taskbar on
    //   Windows; sliding this inner window lets us do that crop with the outer
    //   window.
    Item {
      id: slideWrapper
      x: dashPlacement.directedSlideXOffset
      y: dashPlacement.directedSlideYOffset
      width: dashPlacement.frameWidth
      height: dashPlacement.frameHeight
      visible: true
      opacity: calculatingLayouts ? 0 : slideAnimator.dashOpacity

      // Wrap the content and the tray arrow in an Item so the shadow takes both
      // into account.
      Item {
        id: shadowWrapper
        anchors.fill: parent

        BorderImage {
          anchors.fill: contentItem
          anchors.margins: -10
          border {left: 22; top: 22; right: 22; bottom: 22}
          horizontalTileMode: BorderImage.Stretch
          verticalTileMode: BorderImage.Stretch
          source: Theme.dashboard.shadowImage
        }

        Item {
          z: 5
          id: contentItem
          x: dashPlacement.wrapperX
          y: dashPlacement.wrapperY
          width: dashPlacement.wrapperWidth
          height: dashPlacement.wrapperHeight

          Behavior on height {
            SmoothedAnimation {
              id: wrapperHeightAnimation
              duration: Theme.animation.normalDuration
              easing.type: Easing.InOutQuad
            }
            enabled: visible && !calculatingLayouts
          }

          Behavior on y {
            SmoothedAnimation {
              duration: Theme.animation.normalDuration
              easing.type: Easing.InOutQuad
            }
            enabled: visible && !calculatingLayouts
          }
        }

        Rectangle {
          id: trayArrow

          color: content.headerTopColor
          x: dashWindow.width / 2 + dashPlacement.windowArrowOffset
          visible: dashPlacement.windowArrowVisible
          y: 0
          height: 20
          width: 20
          z: 2
          transform: [
            Rotation {
              angle: 45
            }
          ]
        }
      }
    }
  }

  function showAndActivate() {
    // If no control has been focused yet, focus the first active control.
    if(!activeFocusItem) {
      var firstFocus = contentItem.nextItemInFocusChain(true)
      if(firstFocus)
        firstFocus.forceActiveFocus(Qt.ActiveWindowFocusReason)
    }
    animShow()
    activateTimer.start()
    calculatingLayouts = true
  }

  Timer {
    id: activateTimer
    interval: 10
    running: false
    repeat: false
    onTriggered: {
      // We've had an interval for Qt to calculate window layouts, reveal the
      // window now
      calculatingLayouts = false
      // Both raise() and requestActivate() are needed.  Basically, raise() does
      // what we want on OS X, and requestActivate() does it on Windows.
      //
      // On Windows, raise() would only bring the window above our application's
      // other windows, it doesn't bring it to the foreground on the desktop.
      // requestActivate() does that (and Windows allows it because we're already
      // the foreground application due to the tray icon click).
      //
      // raise() seems to be redundant on Windows, but on OS X, requestActivate()
      // by itself has no effect.
      raise()
      requestActivate()
    }
  }

  // DashboardWindow and DashboardPopup expose a few common methods/properties-
  // showDashboard(), showFromTrayMenu(), trayClicked(), contentRadius, and
  // layoutPageHeight
  function showDashboard(metrics) {
    dashPlacement.likelyExpandHeight = content.maxPageHeight
    dashPlacement.trayIconBound = metrics.trayIconBound
    dashPlacement.screenBound = metrics.screenBound
    dashPlacement.workAreaBound = metrics.workAreaBound
    dashPlacement.screenScale = metrics.screenScale

    showAndActivate()
  }

  Timer {
    id: showFromMenuTimer
    repeat: false
    interval: 1
    onTriggered: dashWindow.showDashboard(TrayIcon.getIconMetrics())
  }
  // Show the dashboard from the tray menu item.  Used on Linux for the
  // "Show Window" menu item that's always visible, used on other platforms for
  // the "Login" menu item.
  function showFromTrayMenu() {
    // Some tray implementations on Linux take focus back right after handling
    // the menu item.  (Observed on Cinnamon, MATE, and Debian GNOME, but not on
    // Ubuntu GNOME or any other environments.)
    //
    // Defer the dashboard activation slightly; otherwise we would show and
    // immediately hide due to the tray taking focus back.
    showFromMenuTimer.start()
  }

  // Focus the dashboard only if it's already visible.
  function focusIfVisible() {
    if(visible) {
      raise()
      requestActivate()
    }
  }

  function trayClicked(metrics) {
    // If it's currently shown as a popup, hide it.  If it's not shown or
    // shown as a window, show it as a popup.
    if(!visible) {
      // On Mac, the settings window doesn't have a Dock icon, so raise it if it
      // was left open.  We don't really want this on other platforms; they show
      // taskbar icons for the settings window.
      //
      // The logic for this is pretty touchy.  We want the dashboard to end up
      // with the focus right now, but we have be sure requestActivate() is
      // called on the settings window (via showSettings()), otherwise on Mojave
      // it ends up in a state where it can't receive the keyboard focus.
      //
      // (This might have something to do with the fact that requestActivate()
      // also sets the underlying QNSView as the first responder for the window,
      // and it appears that QNSView is involved in handling the cursor events
      // that might end up focusing the window.)
      //
      // This doesn't seem to happen on High Sierra at all.  It also doesn't
      // happen on Mojave if we don't enable LSUIElement in the Info.plist,
      // probably Mac OS focuses the window for us instead of Qt having to do it
      // in that case (?).
      //
      // Despite all of the above, we still have to show the dashboard before
      // the settings window, or it will _still_ get stuck.  (The dashboard
      // still ends up with the keyboard focus since its activation is
      // deferred for one frame.)
      showDashboard(metrics)
      // Note that this can occur for the 'splash' dashboard too and
      // 'settingsWindow' doesn't exist yet at that point.
      if(Qt.platform.os === 'osx' && settingsWindow && settingsWindow.visible) {
        settingsWindow.showSettings()
      }
    }
    else
      animHide()
  }

  // This is the padding margin added around the dashboard.  It's mainly for the
  // drop shadow, but it also has an effect during the slide-out animation on
  // Windows, so we apply it even when shaping with a window mask.
  readonly property int windowPadding: Theme.dashboard.windowPadding
  // This is the corner radius that the content should use.  When shaping with
  // the mask, no content rounding is needed, the mask clips off the corners.
  readonly property int contentRadius: shapeWithMask ? 0 : Theme.dashboard.windowRadius
  // The current layout height is the height of the wrapper (but not animated
  // like contentItem.height).
  readonly property int layoutHeight: dashPlacement.wrapperHeight
  // Suppress notifications when the popup-mode dashboard is visible (even if
  // it's not focused, such as when the settings window is focused).
  readonly property bool suppressNotifications: visible

  Connections {
    target: NativeHelpers
    function onAppFocusLost() {
      animHide()
    }
  }

  Shortcut {
    sequence: StandardKey.Close
    context: Qt.WindowShortcut
    onActivated: animHide()
  }

  // When the dashboard is shown/hidden, update UIState
  onVisibleChanged: Client.uiState.dashboard.shown = dashWindow.visible

  Component.onCompleted: {
    NativeHelpers.initDashboardPopup(dashWindow)

    // If the dashboard was previously visible, show the new one.  This happens
    // if the frame is changed while the dashboard is visible, or if the splash
    // dashboard had been shown by the user before we connected, and the
    // connection has now been established.
    if(Client.uiState.dashboard.shown)
      showDashboard(TrayIcon.getIconMetrics())
  }
}
