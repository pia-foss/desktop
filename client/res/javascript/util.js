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

.pragma library
var WALK_SUCCESS = 100;
var WALK_FAIL = 101;
var walk = function (parts, obj, done) {
  if (parts.length === 1) {
    if (obj.hasOwnProperty(parts[0])) {
      done(WALK_SUCCESS, obj[parts[0]]);
    } else {
      done(WALK_FAIL, null);
    }
    return;
  }

  var active = parts.splice(0, 1);
  if (obj.hasOwnProperty(active)) {
    walk(parts, obj[active], done)
  } else {
    done(WALK_FAIL, null)
  }
}

var has = function (obj, key) {
  var foundFlag = true;

  walk(key.split('.'), obj, function (res) {
    if (res === WALK_FAIL) {
      foundFlag = false;
    }
  })

  return foundFlag;
}

var get = function (obj, key) {
  var found = null

  walk(key.split('.'), obj, function (res, val) {
    if (res === WALK_FAIL) {
      found = defaultVal;
    } else {
      found = val;
    }
  });

  return found;
}

// Hack for lack of Array.includes() in V4
var arrayIncludes = function (array, value) {
  return array.findIndex(function(item){return item === value}) > -1
}

// Substitute for Number.isFinite() (ES6)
var isFiniteNumber = function (val) {
  return typeof val === 'number' && isFinite(val);
}

// V4 does not support Sets.  This is a crude approximation used to hold
// references to objects to keep them from being garbage collected (see
// Daemon.pendingResults).
function RefHolder() {
  // The objects are stored in an array.  This means insertion and deletion are
  // O(N), but we never expect to have more than a few results outstanding, so
  // this is OK.  To handle large numbers of results, we'd probably have to
  // store them as properties of an object with index keys, and return the index
  // as a handle to use to remove the object.
  var refs = []
  this.storeRef = function(value) {
    refs.push(value)
  }
  this.releaseRef = function(value) {
    var index = refs.indexOf(value)
    if(index >= 0)
      refs.splice(index, 1) // Delete the item from this position
  }
}

// Trim a semantic version string down to a human-readable short version.
function shortVersion(version) {
  var v = version.split('+', 1)[0].split('-');
  v[0] = v[0].replace(/\.0$/, '');
  return v.join(' ');
}

// Extract plain text from HTML by stripping HTML tags and entities.
// This is very crude, it just:
// - replaces <br> with a space
// - strips <...>
// - strips &...;
// It's used to generate accessibility annotations for some controls which use
// HTML for basic formatting.
function stripHtml(markup) {
  // Replace <br> tags with a space.  Don't smash the adjacent text right
  // together since they're supposed to be separate paragraphs.  (This makes a
  // big difference, VoiceOver for example reads "... has expired.You can..." as
  // "... has expired dot you can ..." with no pause at all, like a URI.  It
  // reads "... has expired. You can ..." correctly.)
  var text = markup.replace(/<br[ \/]*>/g, ' ')
  // Strip other HTML tags and entities.
  text = text.replace(/<[^>]+>/g, '')
  text = text.replace(/&[^;]+;/g, '')
  return text
}

// Ensure a range in a scroll view is visible in either the horizontal or
// vertical direction.  Updates the position of scrollBar.
//
// Params:
// - contentSize - either contentWidth/contentHeight of the scroll view
// - viewSize - either width/height of the scroll view
// - scrollBar - either scrollView.ScrollBar.vertical or
//               scrollView.ScrollBar.horizontal
// - rangeStart - x/y coordinate of left/top of range to bring into view
// - rangeSize - width/height of range to bring into view
function ensureScrollViewRangeVisible(contentSize, viewSize, scrollBar,
                                      rangeStart, rangeSize) {
  // Bottom scroll limit - the farthest we can scroll down, which is the
  // content height minus the view height, unless the content is shorter than
  // the view.
  var bottomScrollLimit = Math.max(0, contentSize - viewSize)

  // The 'maximum' scroll position would put the top of the row just at the
  // top of the view
  var maxScrollPos = rangeStart
  // The max could be out the scroll range if the item is near the bottom
  // (we can't scroll far enough to put that item at the top)
  maxScrollPos = Math.min(bottomScrollLimit, maxScrollPos)
  // The 'minimum' scroll position would put the bottom of the row just at
  // the bottom of the view
  var minScrollPos = rangeStart - viewSize + rangeSize
  // That could be out of the scroll range if the item is near the top
  minScrollPos = Math.max(0, minScrollPos)

  // Normalize the scroll positions.
  maxScrollPos /= contentSize
  minScrollPos /= contentSize

  // Compute the final scroll position by clamping the current scroll
  // position to those limits.
  // As long as the item is not taller than the scroll view,
  // maxScrollPos > minScrollPos.  In case it is taller than the view, apply
  // the max last, so it takes precedence and we prefer to put the top of
  // the item at the top of the view.
  var finalScrollPos = scrollBar.position
  finalScrollPos = Math.max(minScrollPos, finalScrollPos)
  finalScrollPos = Math.min(maxScrollPos, finalScrollPos)

  scrollBar.position = finalScrollPos
}

// Ensure that a horizontal range in a ScrollView is visible.
// Sets the horzScrollBar's position to the new position.
// Params:
// - scrollView - The ScrollView containing the range
// - horzScrollBar - The ScrollView's horizontal scroll bar
//   (scrollView.ScrollBar.horizontal, QtQuick.Controls must be imported)
// - rangeX - Left X-coordinate of the range
// - rangeWidth - Width of the range
function ensureScrollViewHorzVisible(scrollView, horzScrollBar, rangeX, rangeWidth) {
  ensureScrollViewRangeVisible(scrollView.contentWidth, scrollView.width,
                               horzScrollBar, rangeX, rangeWidth)
}

// Ensure that a vertical range in a ScrollView is visible.
// Sets the vertScrollBar's position to the new position.
// Params:
// - scrollView - The ScrollView containing the range
// - vertScrollBar - The ScrollView's vertical scroll bar
//   (scrollView.ScrollBar.vertical, QtQuick.Controls must be imported)
// - rangeY - Top Y-coordinate of the range
// - rangeHeight - Height of the range
function ensureScrollViewVertVisible(scrollView, vertScrollBar, rangeY, rangeHeight) {
  ensureScrollViewRangeVisible(scrollView.contentHeight, scrollView.height,
                               vertScrollBar, rangeY, rangeHeight)
}

// Ensure that a bounding rectangle inside a ScrollView is visible.
// Updates the scroll bars' positions.
// Params:
// - scrollView - The ScrollView containing the range
// - horzScrollBar - The ScrollView's horizontal scroll bar
//   (scrollView.ScrollBar.horizontal, QtQuick.Controls must be imported)
// - vertScrollBar - The ScrollView's vertical scroll bar
//   (scrollView.ScrollBar.vertical, QtQuick.Controls must be imported)
// - bound - a QML `rect` defining the bounds to be brought into view
function ensureScrollViewBoundVisible(scrollView, horzScrollBar, vertScrollBar,
                                      bound)
{
  ensureScrollViewHorzVisible(scrollView, horzScrollBar, bound.x, bound.width)
  ensureScrollViewVertVisible(scrollView, vertScrollBar, bound.y, bound.height)
}

// Create a dependency on the absolute position of an Item.  This creates
// dependencies on its position, the positions of its parent Items, and their
// scale transformations.
// (In principle, this should depend on all types of transformations, but scales
// are the only transformations widely used in the client.)
function dependAbsolutePosition(item, window) {
  // Assign the values somewhere just to silence IDE warnings
  var dependency

  // In principle, this should depend on item.scale, item.transformOrigin, and
  // the properties of transformations in item.transforms.
  //
  // However, reading item.transforms results in an outrageous number of
  // warnings from the QML engine, because it doesn't have a NOTIFY signal.
  // (It was probably intended to be constant, but the author forgot CONSTANT).
  //
  // As a workaround, depend on the window's contentScale property instead,
  // which is used to set up scale transformations and is provided by all PIA
  // windows.
  dependency = window ? window.contentScale : undefined

  while(item) {
    dependency = item.x
    dependency = item.y

    item = item.parent
  }
}

// For a given side-specific margin of a Popup (leftMargin, etc.), get the
// effective margin for that side.  Returns the side specific margin if set,
// margins otherwise, or 0 if neither is set.
function getPopupEffectiveMargin(popup, sideMargin) {
  if(sideMargin >= 0)
    return sideMargin
  return Math.max(popup.margins, 0)
}

// Correctly limit and round a Popup's X or Y position.
// Use the X or Y wrapper in any Popup.x/y bindings.
// Fixes:
// - Popup doesn't correctly limit to window edges when the overlay layer is
//   scaled
// - Round Popup positions in scene coordinates to prevent rendering artifacts
//
// Returns the actual coordinate value to pass to Popup.
//
// Parameters:
// - popup - the Popup being positioned
// - window - popup.Window.window
// - overlay - popup.Overlay.overlay
// - sizeFunc - Given an item, return either width or height as appropriate
// - intendedCoord - The desired value for this coordinate
// - coordMapFunc - Map an X or Y coordinate (as appropriate) from one item to
//   another.  function(srcItem, destItem, coord) -> return coord mapped with
//   Item.mapFromItem()
// - minMargin - Left or top margin property value
// - maxMargin - Right or bottom margin property value
function popupCoordBindingFixup(popup, window, overlay, sizeFunc,
                                intendedCoord, coordMapFunc,
                                minMargin, maxMargin) {
    var parent = popup.parent
    if(!parent || !window || !overlay)
      return intendedCoord

    // This computation depends on the absolute position of the parent.  This
    // introduces the dependencies that affect the result of mapFromItem().
    dependAbsolutePosition(parent, window)

    // Get the window bounds relative to the parent item.  (This is in
    // logical coordinates, since the transformation from contentItem to
    // infoTip takes the window scale factor into account.)
    var boundMin = coordMapFunc(window.contentItem, parent, 0)
    var boundMax = coordMapFunc(window.contentItem, parent, sizeFunc(window.contentItem))

    // These might be reversed if a mirror transformation is active on the
    // overlay layer (or anywhere in between the window and popup in general).
    if(boundMax < boundMin) {
      var oldMin = boundMin
      boundMin = boundMax
      boundMax = oldMin
    }

    // Apply margins
    boundMin += getPopupEffectiveMargin(popup, minMargin)
    boundMax -= getPopupEffectiveMargin(popup, maxMargin)

    // Limit to edges.  If the popup doesn't fit, prefer the left/top.
    var limitedCoord = intendedCoord
    limitedCoord = Math.min(limitedCoord, boundMax - sizeFunc(popup))
    limitedCoord = Math.max(limitedCoord, boundMin)

    // Round to an integer.  On *some* backends, non-integer coordinates cause
    // really strange artifacts - see issue #273.  Be careful with this, it
    // renders differently depending on the exact card/driver/backend.
    //
    // We need the _physical_ position to be an integer, not the logical
    // position.  To do that, compute the absolute position using the parent's
    // absolute position, then round the absolute position, then go back to a
    // relative position.
    //
    // Note that this will continue to update if the view is scrolled since we
    // created a dependency on the absolute position of the parent item.
    var popupAbsCoord = coordMapFunc(parent, window.contentItem, limitedCoord)
    popupAbsCoord = Math.round(popupAbsCoord)
    return coordMapFunc(window.contentItem, parent, popupAbsCoord)
}

// Limit and round a Popup's X or Y position.
// Parameters:
// - popup: The Popup being positioned
// - window: The window for the popup, such as popup.parent.Window.window
// - overlay: The window's overlay layer, such as popup.parent.Overlay.overlay
// - intendedX/intendedY: The intended X/Y coordinate, which will be clamped to
//   the window edges
//
// Note that for both window and overlay, the attached properties can't be
// attached to the Popup itself since it's not an Item, they have to be attached
// to the popup's parent (or contentItem, etc.) instead.
function popupXBindingFixup(popup, window, overlay, intendedX) {
  var sizeFunc = function(item) {return item.width}

  var coordMapFunc = function(srcItem, destItem, coord) {
    return destItem.mapFromItem(srcItem, coord, 0).x
  }

  return popupCoordBindingFixup(popup, window, overlay, sizeFunc, intendedX,
    coordMapFunc, popup.leftMargin, popup.rightMargin)
}
function popupYBindingFixup(popup, window, overlay, intendedX) {
  var sizeFunc = function(item) {return item.height}

  var coordMapFunc = function(srcItem, destItem, coord) {
    return destItem.mapFromItem(srcItem, 0, coord).y
  }

  return popupCoordBindingFixup(popup, window, overlay, sizeFunc, intendedX,
    coordMapFunc, popup.topMargin, popup.bottomMargin)
}
// Both X and Y at once - returns a point.  Useful when the point is passed to
// Menu.popup() instead of being used in x/y bindings.
function popupXYBindingFixup(popup, window, overlay, intendedX, intendedY) {
  return Qt.point(popupXBindingFixup(popup, window, overlay, intendedX),
                  popupYBindingFixup(popup, window, overlay, intendedY))
}

// Reading back the position (x/y) of a Popup is buggy - it does not handle
// transformations in the overlay layer correctly.
//
// It maps the actual X in Overlay.overlay's coordinates back to the parent
// item's coordinates as if it was a scene position, which is wrong.
//
// There's no other way to get the actual effective X position, so we have to
// work backward to calculate the correct value.
function calculatePopupActualPos(popup, window, overlay) {
  // Window may be null when the item is being shown or hidden, return anything
  if(!window)
    return Qt.point(popup.x, popup.y)

  // Map the buggy position back to coordinates within Overlay.overlay (undo the
  // incorrect item->scene coordinate mapping done by
  // QQuickPositioner::reposition()).
  //
  // Here it's assumed that window.contentItem coords are equivalent to scene
  // coords, which is only true if the contentItem is not transformed.
  //
  // If the contentItem ever needs to be transformed, we could expose
  // QQuickItem::mapToScene() from a native helper function and use that
  // instead.
  var overlayCoords = window.contentItem.mapFromItem(popup.parent, popup.x, popup.y)
  // Map the overlay coordinates to the correct parent item coordinates
  return popup.parent.mapFromItem(overlay, overlayCoords.x, overlayCoords.y)
}

// Convert a color component to a 2-digit hex string (used by colorToStr())
function compToStr(comp) {
  var comp8bit = Math.round(comp * 255)
  var leadingZero = comp8bit < 16 ? '0' : ''
  return leadingZero + comp8bit.toString(16)
}

// Convert a color to an HTML color string ("#RRGGBB")
function colorToStr(c) {
  return '#' + compToStr(c.r) + compToStr(c.g) + compToStr(c.b)
}
