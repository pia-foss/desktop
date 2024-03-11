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
import "../theme"

// DashboardPositioner just computes the screen position of the dashboard given
// the tray icon and screen bounds.
//
// The final position computed is the screen position of the visible dashboard.
// It does not include padding, although padding is considered when placing it.
// (This is not necessarily the final position actually used to place the
// dashboard; the actual positioning is complex and involves wrapper windows to
// ensure that animations work correctly.  See DashboardPopupPlacement.)
//
// This computation is independent of whether the dashboard is shown as a window
// or popup.  (Historically it was used for the windowed dashboard to put it in
// the same initial position, but this is no longer the case.)
Item {
  // The height desired by the current page.  (DashboardPopup/DashboardWindow
  // bind this to the height expressed by DashboardWrapper.)
  // This does not influence the direction chosen, it just allows us to
  // calculate the actual dashboard Y-position.
  // Since it doesn't influence the direction chosen, the dashboard binds this
  // to move the dashboard around as the page height changes.
  // If this height does not fit in the current work area, the dashboard will be
  // sized to the work area instead, so pages should lay out using the actual
  // dashboard height instead of the desired height.
  property int pageHeight

  //===Placement-determining properties===
  // The following properties determine the show and expand directions for the
  // dashboard.  These are set (not bound) each time the dashboard is about to
  // be shown.
  //
  // They're not bound, because we don't want to move the dashboard around while
  // it is shown.  Note that the "expanded" height is in this group - it is the
  // likely expanded height of the dashboard, not the absolute maximum possible
  // height.  This height can change while shown (as errors are added, modules
  // are bookmarked, etc.), so we do not bind it.
  //
  // This means that all dashboard pages must be prepared to be sized below
  // their expanded size, which is possible anyway because the work area might
  // not be that large.

  // The width of the dashboard (does not change per-page).  This does influence
  // the dashboard position, so it must be constant.
  property int dashWidth
  // The likely expanded height of the dashboard.  This is the likely maximum
  // size of any page, but not an absolute maximum.  (The absolute maximum
  // would in general be very tall, since it's possible the user could bookmark
  // all modules and have a bunch of errors, etc.)
  property int likelyExpandHeight

  // These properties depend on the last tray icon clicked by the user.  (There
  // can be more than one on Mac OS or Linux.)  We expect DashboardPopup /
  // DashboardWindow to assign these when a click occurs, so we just fill in
  // some default values to avoid warnings before these values are known.
  // The position of the last tray icon that we showed the dashboard from.
  property rect trayIconBound: Qt.rect(200, 200, 16, 16)
  // The position of the screen bound that contained that tray icon.
  property rect screenBound: Qt.rect(0, 0, 300, 300)
  // The available geometry on that screen, excluding menus, taskbars, etc.
  property rect workAreaBound: Qt.rect(0, 0, 300, 300)
  // The scaling factor to use when rendering to the screen given by screenBound
  property real screenScale: 1

  // The tray icon bound and screen bound are given in virtual screen
  // coordinates.  Scale them to logical coordinates, then do all the placement
  // work in logical coordinates.  The actual window (DashboardWindow or
  // DashboardPopup) will scale some of these logical coordinates back to
  // virtual screen coordinates if they're applied to a property defined that
  // way.
  function scaleRect(rectValue, scale) {
    return Qt.rect(rectValue.x * scale, rectValue.y * scale,
                   rectValue.width * scale, rectValue.height * scale);
  }
  readonly property rect trayIconLogBound: scaleRect(trayIconBound, 1/screenScale)
  readonly property rect screenLogBound: scaleRect(screenBound, 1/screenScale)
  readonly property rect workAreaLogBound: scaleRect(workAreaBound, 1/screenScale)

  // This is the maximum window padding used by either DashboardWindowPlacement
  // or DashboardPopupPlacement.  The maximum padding is considered in order to
  // provide a consistent position, but no padding is included in the computed
  // bounds.
  readonly property int maxPadding: Theme.dashboard.windowPadding

  // Gap between the dashboard edge and the tray icon edge.
  // On OS X, this doesn't have any effect, the tray arrow is larger than this,
  // and OS X won't let us position the window outside of the work area (which
  // we rely on to put the tray icon right against the menu bar).
  readonly property int taskbarMargin: 8
  // Margin between edge of dashboard and edge of screen
  readonly property int horzEdgeMargin: 8

  // Constants for showDirection / expandDirection
  readonly property var directions: {
    return {
      down: 'down',
      up: 'up',
      right: 'right',
      left: 'left'
    }
  }

  // Maximum dashboard width / height used to evaluate fit
  readonly property int maxDashWidth: dashWidth + 2*maxPadding
  readonly property int maxLikelyExpandHeight: likelyExpandHeight + 2*maxPadding

  // These are the fit bounds for each possible placement direction - the amount
  // of screen space we have to work with in that direction.  We use them all to
  // choose a show direction, and then we'll use the actual fit bound based on
  // that choice to place the dashboard.
  readonly property rect fitBoundUp: {
    // The bottom of the usable space is either the work area bottom or icon top
    var bottom = Math.min(trayIconLogBound.top, workAreaLogBound.bottom)
    return Qt.rect(workAreaLogBound.x, workAreaLogBound.y,
                   workAreaLogBound.width, bottom - workAreaLogBound.y)
  }
  readonly property rect fitBoundDown: {
    // The top of the usable space is either the work area top or icon bottom
    var top = Math.max(trayIconLogBound.bottom, workAreaLogBound.top)
    return Qt.rect(workAreaLogBound.x, top,
                   workAreaLogBound.width, workAreaLogBound.bottom - top)
  }
  readonly property rect fitBoundLeft: {
    // The right edge of the usable space is either the work area right edge or
    // icon left edge
    var right = Math.min(trayIconLogBound.left, workAreaLogBound.right)
    return Qt.rect(workAreaLogBound.x, workAreaLogBound.y,
                   right - workAreaLogBound.x, workAreaLogBound.height)
  }
  readonly property rect fitBoundRight: {
    // The left edge of the usable space is either the work area left edge or
    // icon right edge
    var left = Math.max(trayIconLogBound.right, workAreaLogBound.left)
    return Qt.rect(left, workAreaLogBound.y,
                   workAreaLogBound.right - left, workAreaLogBound.height)
  }

  // Clamp a value to the range [min, max]
  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value))
  }

  // Compute the fraction of the dashboard that would fit in a given rectangle.
  // If the client fully fits in that direction (when expanded), the fit is 1.0.
  // If the client doesn't fit at all in that direction, the fit is 0.0.  If the
  // client partially fits, it is somewhere in between.
  function calcFitFraction(availableRect) {
    // Compute the area of the dashboard that would be visible.
    var visibleHeight = clamp(availableRect.bottom - availableRect.top, 0,
                              maxLikelyExpandHeight)
    var visibleWidth = clamp(availableRect.right - availableRect.left, 0,
                             maxDashWidth)
    var visibleArea = visibleHeight * visibleWidth;
    // The fit fraction is that area divided by the total area.  It shouldn't be
    // outside the range [0.0, 1.0] due to the clamping above, but clamp again
    // due to possible roundoff error.  (The algorithm used for showDirection
    // relies on this being exactly 1.0 for all directions that fully fit the
    // dashboard.)
    return clamp(visibleArea / (maxDashWidth * maxLikelyExpandHeight), 0, 1)
  }

  // The direction to animate when showing the dashboard (and which side of the
  // icon the dashboard appears on).
  // (directions.down means: place below tray icon, animate downward when
  // showing, animate upward when hiding, etc.)
  readonly property string showDirection: {
    // If the tray icon is outside of the work area on one side, choose the
    // obvious direction.
    var trayInWorkX = (trayIconLogBound.left >= workAreaLogBound.left &&
                       trayIconLogBound.right <= workAreaLogBound.right)
    var trayInWorkY = (trayIconLogBound.top >= workAreaLogBound.top &&
                       trayIconLogBound.bottom <= workAreaLogBound.bottom)
    // These possibilities are mutually exclusive, the order here doesn't
    // matter.
    var placement;
    if(trayInWorkX && trayIconLogBound.bottom <= workAreaLogBound.top)
      placement = directions.down
    if(trayInWorkX && trayIconLogBound.top >= workAreaLogBound.bottom)
      placement = directions.up
    if(trayInWorkY && trayIconLogBound.right <= workAreaLogBound.left)
      placement = directions.right
    if(trayInWorkY && trayIconLogBound.left >= workAreaLogBound.right)
      placement = directions.left

    if(placement) {
      console.log('Chose show direction ' + placement + ' due to icon location outside of work area')
      return placement
    }

    // Otherwise, the icon is actually inside the work area, or it's outside of
    // it in a corner that doesn't give an obvious placement choice.  Choose a
    // direction to show the dashboard based on where it will fit.
    //
    // This happens on Windows when the icon appears in a popup (when the tray
    // hides it).  The popup will be somewhere in the middle of the tray.  It's
    // also useful for Linux, where the icon could be anywhere on the screen,
    // and we may not know the work area bound at all (it'd be equal to the
    // screen bound).
    //
    // An additional input we're ignoring is the actual location of the taskbar
    // on Windows.  We could figure this out from the work area, but it wouldn't
    // really produce a result that's always "better" - if the icon popup is
    // large, we really might need to choose a show direction different from the
    // taskbar's obvious direction.
    //
    // This algorithm chooses a reasonable direction for any possible placement.
    // - If exactly one direction fully fits, choose that one.
    // - If no direction fully fits, choose the one that shows the most of the
    //   dashboard.
    // - If more than one direction fully fits, we prefer down, then up, then
    //   left.
    // - If no direction fully fits, and more than one direction is tied for the
    //   best fit, we still prefer down, up, then left.
    var fitDown = calcFitFraction(fitBoundDown)
    var fitUp = calcFitFraction(fitBoundUp)
    var fitRight = calcFitFraction(fitBoundRight)
    var fitLeft = calcFitFraction(fitBoundLeft)
    var bestFit = fitRight
    var bestDirection = directions.right
    if(fitLeft >= bestFit) {
      bestFit = fitLeft
      bestDirection = directions.left
    }
    if(fitUp >= bestFit) {
      bestFit = fitUp
      bestDirection = directions.up
    }
    if(fitDown >= bestFit) {
      bestFit = fitDown
      bestDirection = directions.down
    }

    console.log('Chose show direction ' + bestDirection +
                ' with fit fraction ' + bestFit)

    return bestDirection
  }

  // The direction to expand the dashboard (either up or down, never left or
  // right)
  readonly property string expandDirection: {
    // When showing above or below, always expand that direction.
    if(showDirection === directions.up || showDirection === directions.down)
      return showDirection

    // When showing on the left or right, expand down if the icon is on the top
    // half of the work area, or up if it's on the bottom half.
    var iconVCenter = (trayIconLogBound.top + trayIconLogBound.bottom) * 0.5
    var workVCenter = (workAreaLogBound.top + workAreaLogBound.bottom) * 0.5

    return iconVCenter >= workVCenter ? directions.up : directions.down
  }

  // Actual fit bound based on chosen show direction
  readonly property rect fitBound: {
    switch(showDirection)
    {
      default:
      case directions.down:
        return fitBoundDown
      case directions.up:
        return fitBoundUp
      case directions.right:
        return fitBoundRight
      case directions.left:
        return fitBoundLeft
    }
  }

  // The actual screen X-coordinate for the dashboard based on the tray icon
  // bound, screen bound, and current dashboard height.
  readonly property int dashScreenX: {
    switch(showDirection)
    {
      default:
      case directions.down:
      case directions.up:
        // If possible, center the dash on the icon.
        var newX = trayIconLogBound.x + trayIconLogBound.width / 2 - maxDashWidth / 2
        // Clamp to fit bounds.
        var leftXLimit = fitBound.left + horzEdgeMargin
        var rightXLimit = fitBound.right - horzEdgeMargin - maxDashWidth
        newX = Math.min(newX, rightXLimit)
        newX = Math.max(newX, leftXLimit)

        // Remove the padding offset
        newX += maxPadding

        return newX
      case directions.right:
        // Use the left edge of the fit bound
        return fitBound.left + maxPadding
      case directions.left:
        // Use the right edge of the fit bound
        return fitBound.right - maxDashWidth + maxPadding
    }
  }

  // The actual maximum height for the dashboard (not including padding) based
  // on the placement, work area, and tray icon position.  The dashboard cannot
  // ever exceed this height in this placement, or it would extend beyond the
  // edge of the screen.
  readonly property int dashActualMaxHeight: {
    // The max height is the height of the fit bound excluding the taskbar
    // margin
    return (fitBound.bottom - fitBound.top) - taskbarMargin
  }

  // The actual dashboard height (not including padding) is the page height if
  // it fits, otherwise it's the maximum height that does fit.
  readonly property int dashActualHeight: {
    return Math.min(pageHeight, dashActualMaxHeight)
  }

  // The actual screen Y-coordinate for the dashboard based on the tray icon
  // bound, screen bound, and current dashboard height.
  // When popped below, the top edge is fixed.  When popped above, the bottom
  // edge is fixed; the top is computed from the height.
  readonly property int dashScreenY: {
    switch(expandDirection)
    {
      default:
      case directions.down:
        // Expand downward.  The top edge is fixed at the top of the fit bound.
        return fitBound.top + taskbarMargin
      case directions.up:
        // Expand upward.  The bottom edge is fixed at the bottom of the fit
        // bound.
        // The actual Y coordinate of the top edge depends on the current page
        // height
        return fitBound.bottom - taskbarMargin - dashActualHeight
    }
  }

  // Debug tracing for dash position
  readonly property var traceDashPos: {
    console.info('dash screen pos: ' + dashScreenX + ',' + dashScreenY + ':' + dashActualHeight)
    return
  }
}
