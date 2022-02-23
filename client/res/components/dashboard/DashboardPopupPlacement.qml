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
import QtQuick.Window 2.10
import PIA.NativeHelpers 1.0
import "../theme"


// DashboardPopupPlacement calculates the placement of the dashboard based on
// the tray icon / screen bounds and whether it is expanded.
//
// See DashboardPositioner for input properties that DashboardPopup is expected
// to bind or set.
DashboardPositioner {
  // When shaping with a window mask instead of alpha blending, the mask height
  // is computed differently.
  property bool shapeWithMask

  // When the dashboard's height is animating, the current animating height.
  // (The heights of the dashboard content, in logical coordinates.)  0 if the
  // height is not currently animating.
  property int dashAnimatingHeight
  // Vertical offset to apply for the slide animation.  DashboardPopup animates
  // this during its show / hide transitions.
  property real slideOffset

  // Padding between dashboard window / dashboard wrapper (for drop shadow)
  property int windowPadding

  // The height for the dashboard.  (Includes padding.)
  readonly property int windowHeight: dashActualHeight + 2*windowPadding
  // Max possible height for the dashboard.  (Includes padding.)
  readonly property int windowMaxHeight: dashActualMaxHeight + 2*windowPadding
  // Width for the dashboard (includes padding)
  readonly property int windowWidth: dashWidth + 2*windowPadding
  // Whether the dashboard arrow should be shown (shown when popped below only)
  readonly property bool windowArrowVisible: {
    // Show the arrow only when popped below.
    if(showDirection !== directions.down)
      return false

    // Only show the arrow if it would actually point to our icon.  If the
    // taskbar shows more than one row of icons, we put the dashboard at the
    // edge of the taskbar.  If we're not the closest icon to the edge, the
    // arrow would point to some other icon, which is a bit weird.
    var halfIconHeight = (trayIconLogBound.bottom - trayIconLogBound.top) * 0.5
    if(fitBound.top <= trayIconLogBound.bottom + halfIconHeight)
      return true

    return false
  }

  // When animating, the animating height for the window. (Includes padding.)
  readonly property int windowAnimatingHeight: {
    if(dashAnimatingHeight)
      return dashAnimatingHeight + 2*windowPadding
    return 0
  }


  // Position for the dashboard when popped from the tray icon.  (These include
  // padding.)
  readonly property int windowPoppedX: dashScreenX - windowPadding
  readonly property int windowPoppedY: dashScreenY - windowPadding

  // The dashboard is *actually* placed using an invisible outer window
  // (DashboardPopup), and an inner wrapper window that moves within it
  // (DashboardWrapper).  When the dashboard expands, the outer window does not
  // move, just the inner window moves within it.
  //
  // This is particularly important when the dashboard expands upward (Windows),
  // it's the only way to get the top edge to move correctly since we have to
  // animate top and height in lockstep.
  //
  // Fortunately, on Windows, we can set a window mask (AKA a window region) to
  // mask off the invisible part of the outer window.
  //
  // On Mac OS though, the outer window *can't* be clicked through, and setting
  // the mask has no effect.  So on Mac OS, we have to actually collapse the
  // outer window after the dash collapses.

  // The outer window's X is the same, no fancy business here.
  readonly property int frameX: windowPoppedX
  // The outer window's Y is the same when expanding downward.  When expanding
  // upward, it is extended so it can show the expanded dashboard.
  readonly property int frameY: {
    if(expandDirection === directions.up)
      return fitBound.bottom - taskbarMargin - frameHeight + windowPadding
    else
      return windowPoppedY
  }
  // Again, no funny business on width.
  readonly property int frameWidth: windowWidth
  // The frame height depends on the platform.
  readonly property int frameHeight: {
    if(Qt.platform.os === 'osx') {
      // For OS X only, use the correct height, but keep the maximum height when
      // the height is animating.  (Don't bother updating continuously during
      // the animation, this isn't really necessary on OS X and might have a
      // performance cost.)
      //
      // On OS X, the invisible part of the window *does* eat cursor events, but
      // tweaking the Y position does not cause artifacts (the window manager on
      // OS X is synchronous).
      //
      // Technically this leaves an invisible-but-interactive part of the window
      // up during the animation, but the animation doesn't last long.
      return windowAnimatingHeight ? windowMaxHeight : windowHeight
    }
    else {
      // On Windows and Linux, always use the max height.  Changing the size
      // causes the contents of the window to jump around (the window size,
      // mask, and contents are all unsynchronized), but the invisible part is
      // not interactive.
      return windowMaxHeight
    }
  }

  // Trace frame pos
  readonly property var traceFramePos: {
    console.info('dash frame pos: ' + frameX + ',' + frameY + ':' + frameWidth + 'x' + frameHeight)
    return
  }

  // The inner window is positioned inside the margins.  No funny business in X.
  readonly property int wrapperX: windowPadding
  readonly property int wrapperWidth: frameWidth - 2*windowPadding
  // On Y, compute the dash's offset inside the frame using the absolute
  // position we calculated earlier and the position of the frame.
  readonly property int wrapperY: {
    // This difference is only nonzero when above the icon.
    var edgeOffset = windowPoppedY - frameY
    // Add the margin
    return edgeOffset + windowPadding
  }
  readonly property int wrapperHeight: dashActualHeight

  readonly property int directedSlideXOffset: {
    switch(showDirection)
    {
      default:
      case directions.down:
      case directions.up:
        return 0
      case directions.right:
        return -slideOffset
      case directions.left:
        return slideOffset
    }
  }
  readonly property int directedSlideYOffset: {
    switch(showDirection)
    {
      default:
      case directions.down:
        return -slideOffset
      case directions.up:
        return slideOffset
      case directions.right:
      case directions.left:
        return 0
    }
  }

  // We set a window mask on the outer window to clip off the transparent area,
  // so cursor events pass through them.
  //
  // Qt considers this a *hint*.  It works on Windows (which is where we need
  // it - it calls ::SetWindowRgn()), but it does not affect the cursor on OS X.
  //
  // Qt does *not* specify whether the window manager will draw the clipped
  // parts, so they must be transparent (which they are).  This is just to get
  // cursor interactions to work right on Windows without moving the outer
  // window, which would cause artifacts.
  //
  // Note that on some backends on Windows, cursor interactions pass through the
  // transparent part without a window mask, but this is necessary for it to
  // work on all backends.
  readonly property rect clip: {
    // Determine the clip height
    // - When alpha blending, use the max height during an animation, no need to
    //   update continuously.  This just leaves an invisible part of the window
    //   that eats cursor events during the animation, since the alpha channel
    //   provides transparency.  The animations are quick so this isn't
    //   noticeable.  (Animating the height could cause clipping artifacts since
    //   the window mask is not synchronized with the window content.)
    // - When shaping with a window mask, this affects the visible part of the
    //   window, since the window mask creates the window shape.  Animate
    //   continuously to render the expansion properly.  The mask isn't
    //   synchronized with the window contents, but this still looks reasonable.
    //   (The background was filled with gray already, and the window doesn't
    //   jump around because it was sized to the work area height.)
    var clipHeight
    if(windowAnimatingHeight)
      clipHeight = shapeWithMask ? windowAnimatingHeight : windowMaxHeight
    else
      clipHeight = windowHeight

    var clipY
    // When expanding up, clip from the bottom edge of the padded window bound
    // using the determined clip height.  (If the clip height is windowHeight,
    // this is equivalent to wrapperY-windowPadding.)
    if(expandDirection === directions.up)
      clipY = wrapperY + wrapperHeight + windowPadding - clipHeight
    // When expanding down, clip from the top edge of the padded window bound.
    else
      clipY = wrapperY - windowPadding

    var clipX = 0
    var clipWidth = windowWidth

    // When shaping with the window mask, offset the window mask to match the
    // dashboard content - apply the slide and padding
    if(shapeWithMask) {
      clipX += directedSlideXOffset + windowPadding
      clipY += directedSlideYOffset + windowPadding
      clipWidth -= 2*windowPadding
      clipHeight -= 2*windowPadding
    }
    return Qt.rect(clipX, clipY, clipWidth, clipHeight)
  }

  // The arrow offset - the offset from the center point of the dashboard to the
  // center of the tray icon.
  readonly property int windowArrowOffset: {
    return (trayIconLogBound.left + trayIconLogBound.width/2) -
           (windowPoppedX + windowWidth / 2)
  }
}
