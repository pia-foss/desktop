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

.pragma library
/*
===keyutil.js===
 This library contains utilities used to implement keyboard navigation for
 various controls.

 Some controls are completely custom and require keyboard navigation from
 scratch (like PrivacyInput); some just require customized navigation
 (DropdownInput - to support individual disabled choices).

==='model' and 'textProp'===
 For many of these functions, the list of choices is represented as a 'model'
 with an optional 'textProp'.  If textProp is undefined, model is an array of
 strings (the display text).  If textProp is set, model is an array of objects,
 and the display text is in the property named by textProp.

 Objects in model can also optionally have a disabled attribute, which if truthy
 prevents that choice from being selected.
*/

// Get a property of an object, or return the object itself if 'prop' is
// undefined
function optionalPropOf(obj, prop) {
  // Only undefined actually indicates there is no property (use the object
  // itself), other falsy values still mean that property ('', false, etc.)
  if(typeof(prop) === 'undefined')
    return obj
  return obj[prop]
}

// Seek to a new selection in a list-like control based on key input, such as
// pressing 'e' in the Language drop-down to select English, etc.
//
// Params:
// - model - Array of choices - see "model and textProp" above
// - textProp - Display text property for the model (or undefined)
// - disabledProp - Property indicating disabled items (for truthy values),
//   or undefined if there is none
// - currentIndex - Currently-selected item in the control
// - keyText - Text representation of the key that was pressed
//
// Returns a new selection index if a match was found (even if it is the same
// item that's already selected).  Returns -1 only if there is no match at all
// for the key pressed.
function seekChoiceByKey(model, textProp, disabledProp, currentIndex, keyText) {
  // TODO - this should use the current locale
  keyText = keyText.toLowerCase()
  var startIndex = 0

  // Does the current selection begin with that letter?  If so, look for the
  // next one instead of starting from the beginning
  if(model[currentIndex]) {
    var currentValue = optionalPropOf(model[currentIndex], textProp)
    if(currentValue.toLowerCase().startsWith(keyText))
      startIndex = (currentIndex + 1) % model.length
  }

  var nextIndex = startIndex
  do {
    // Does this item match the letter, and is it selectable?
    var nextChoice = model[nextIndex]
    var nextName = optionalPropOf(nextChoice, textProp)
    if(nextName.toLowerCase().startsWith(keyText)) {
      // It matches, select if it's enabled (including if no disabled property
      // was given)
      if(typeof(disabledProp) === 'undefined' || !nextChoice[disabledProp])
        return nextIndex
    }

    // Go on to the next one, wrap around if we reach the end
    nextIndex = (nextIndex+1) % model.length
  }
  while(nextIndex != startIndex)  // Terminate when reaching the start again

  return -1 // No match for this key
}

// Find the next selection in a direction - used to implement down/right
// (direction=+1), up/left (direction=-1), and home/end (currentIndex=-1)
//
// currentIndex is -1 for Home/End, but it could also be -1 for an arrow key if
// there was no selection in the control.  (The desired behavior is the same -
// for example "right" on a control with no selection searches from the
// beginning.)
//
// If currentIndex is -1, the search starts from the beginning or end of the
// choices, depending on direction.
//
// Returns a new index, which might be currentIndex if the selection remains at
// one end of the control.  Always returns a valid index if currentIndex was
// valid.  If currentIndex is -1 and there are no valid choices in the control,
// returns -1.
function seekSelection(direction, model, currentIndex) {
  var nextIndex
  if(currentIndex === -1) {
    // Start from one end of the control (including that end if it's valid)
    if(direction > 0)
      nextIndex = -1 + direction
    else
      nextIndex = model.length + direction
  }
  else {
    // Start from currentIndex (not including currentIndex itself)
    nextIndex = currentIndex + direction
  }

  while(nextIndex >= 0 && nextIndex < model.length) {
    if(!model[nextIndex].disabled) {
      return nextIndex
    }
    nextIndex += direction
  }
  return currentIndex
}

// Find the next selection from a starting point - used to implement Home/End.
//
// This differs from seekNextSelection() in that the starting index is chosen if
// it's a valid choice - seekNextSelection() moves to the next selection in the
// specified direction instead if there is one.
//
//
// If there aren't any valid choices in the control at all, this returns -1,
// otherwise it returns the first

// Handle a key event for an arbitrary control - handles a pair of directional
// keys and letter navigation.
//
// Use this to implement Keys.onPressed.  handleKeyEvent accepts the event if
// necessary.  If it returns a valid index (which indicates that the event was
// accepted), the control should do the following:
// - Select the new index (if it changed, it might stay the same)
// - Reveal focus cues (even if the index did not change)
//
// Params:
// - keyEvent - The KeyEvent from Keys.onPressed.  The event is accepted if the
//   key is meaningful for this control.
// - model - Array of choices - see "model and textProp" above
// - textProp - Display text property for the model (or undefined)
// - currentIndex - Currently-selected item in the control, can be -1 for no
//     selection
// - forwardKey - Qt key identifier for the 'forward' arrow - down or right
// - reverseKey - Qt key identifier for the 'reverse' arrow - up or left
//
// Returns the new index if the event was accepted (even if it's still
// currentIndex), or -1 if the event is not meaningful to this control.
function handleKeyEvent(keyEvent, model, textProp, currentIndex, forwardKey,
                        reverseKey) {
  // For home/end, go to the first or last valid selection.  Do that by seeking
  // from the beginning or end instead of from currentIndex
  if(keyEvent.key === Qt.Key_Home) {
    keyEvent.accepted = true
    // Seek forward from the beginning
    return seekSelection(1, model, -1)
  }
  else if(keyEvent.key === Qt.Key_End) {
    keyEvent.accepted = true
    // Seek backward from the end
    return seekSelection(-1, model, -1)
  }
  // For arrows, seek to the next selection in that direction (or from an end if
  // there is no selection)
  else if(keyEvent.key === reverseKey) {
    keyEvent.accepted = true
    return seekSelection(-1, model, currentIndex)
  }
  else if(keyEvent.key === forwardKey) {
    keyEvent.accepted = true
    return seekSelection(1, model, currentIndex)
  }
  else if(keyEvent.text) {
    // If the key is a letter, look for a choice starting with that letter
    // This seems to match what Qt does (it doesn't seem to support any IMEs
    // for this)
    var nextIndex = seekChoiceByKey(model, textProp, 'disabled', currentIndex,
                                    keyEvent.text)
    if(nextIndex !== -1) {
      // We found a match; accept the event.
      // If no choice matches, we allow the event to continue propagating,
      // since we didn't do anything.  In particular, this allows
      // Tab/Shift+Tab to be handled correctly (since no choice should
      // begin with a tab/backtab character); there may be other keys like
      // this too.
      keyEvent.accepted = true
      return nextIndex
    }
  }

  return -1
}

// Handle a key event for a vertical control - uses up and down arrows.
// See handleKeyEvent().
function handleVertKeyEvent(keyEvent, model, textProp, currentIndex) {
  return handleKeyEvent(keyEvent, model, textProp, currentIndex, Qt.Key_Down,
                        Qt.Key_Up)
}

// Handle a key event for a horizontal control - uses left and right arrows.
// See handleKeyEvent().
function handleHorzKeyEvent(keyEvent, model, textProp, currentIndex) {
  return handleKeyEvent(keyEvent, model, textProp, currentIndex, Qt.Key_Right,
                        Qt.Key_Left)
}

// Handle a key event for a button-like control.  Responds to Enter or Space.
// Returns true if the control should activate, false otherwise.  Accepts
// keyEvent when returning true.
function handleButtonKeyEvent(keyEvent) {
  // Qt for some reason uses 'Return' to refer to the usual Enter key and
  // 'Enter' to refer to the one on the numeric keypad.  Both should work for
  // this type of control.
  if(keyEvent.key === Qt.Key_Return || keyEvent.key === Qt.Key_Enter ||
     keyEvent.key === Qt.Key_Space) {
    keyEvent.accepted = true
    return true
  }

  return false
}

// Fix up button press events for a button-like control provided by Qt.
//
// Qt already handles spacebar events, and it will handle it even if we do.  It
// doesn't reveal the focus cue though, so for spacebar events this will reveal
// the focus cue.  It still returns false, since Qt handles the button press.
//
// For other press events that Qt doesn't handle (Enter/Return), this reveals
// the focus cue and returns true so the caller can press the button.
//
// focusCue is optional - most controls have this, but menu items do not.
function handlePartialButtonKeyEvent(keyEvent, focusCue) {
  if(keyEvent.key === Qt.Key_Space) {
    if(focusCue)
      focusCue.reveal()
    return false  // Qt will handle the event
  }
  else if(handleButtonKeyEvent(keyEvent)) {
    if(focusCue)
      focusCue.reveal()
    return true // Caller handles the event
  }
  return false
}

// Handle a drop event for a drop-down list control - Alt+Down arrow or F4.
// Returns true if the control should open, false otherwise.  Accepts
// keyEvent when returning true.
// Controls using this should normally also use handleVertKeyEvent().
function handleDropDownKeyEvent(keyEvent) {
  // The Alt+Down key sequence is the same on all platforms (even Mac!)
  // F4 does not apply on Mac though.
  var allowF4 = Qt.platform.os !== 'osx'
  if((keyEvent.key === Qt.Key_Down && (keyEvent.modifiers & Qt.AltModifier)) ||
     (allowF4 && keyEvent.key === Qt.Key_F4 && keyEvent.modifiers === Qt.NoModifier)) {
    keyEvent.accepted = true
    return true
  }
  return false
}

// Handle Settings tab navigation events (only).
// Returns a new index (possibly still currentIndex) if this was a tab event,
// -1 otherwise.  Accepts keyEvent if appropriate.
function handleSettingsTabKeyEvent(keyEvent, currentIndex, tabCount) {
  // This is Ctrl+Tab / Ctrl+Shift+Tab normally, but the Mac equivalent is _not_
  // Cmd+<Shift+>Tab, that's the window switcher.  Instead it's normally
  // Cmd+Shift+[ or Cmd+Shift+].  Keep in mind that Qt swaps Ctrl/Cmd for us so
  // it still appears to be Ctrl.
  var normalKeys = {
    forwardKey: Qt.Key_Tab,
    forwardMod: Qt.ControlModifier,
    // Qt turns this into a Backtab since it has Shift.  The Shift modifier is
    // still reported.  It doesn't seem to do this for bracket/brace though.
    backwardKey: Qt.Key_Backtab,
    backwardMod: Qt.ControlModifier|Qt.ShiftModifier
  };
  var macKeys = {
    forwardKey: Qt.Key_BracketRight,
    forwardMod: Qt.ControlModifier|Qt.ShiftModifier,
    backwardKey: Qt.Key_BracketLeft,
    backwardMod: Qt.ControlModifier|Qt.ShiftModifier
  }
  var keys = (Qt.platform.os === 'osx') ? macKeys : normalKeys

  var direction = 0
  if(keyEvent.key === keys.forwardKey && keyEvent.modifiers === keys.forwardMod)
    direction = 1
  else if(keyEvent.key === keys.backwardKey && keyEvent.modifiers === keys.backwardMod)
    direction = -1
  else
    return -1 // Don't care about this key; ignore

  var nextIndex = currentIndex + direction
  nextIndex = Math.max(0, nextIndex)
  nextIndex = Math.min(tabCount-1, nextIndex)
  keyEvent.accepted = true
  return nextIndex
}

// Clamp a scroll position to a scroll view's limits.
// Returns the scroll position if it's allowed, or the nearest limit otherwise.
function clampScrollPos(position, contentSize, viewSize)
{
  var endScrollLimit = Math.max(0, contentSize - viewSize) / contentSize
  position = Math.min(endScrollLimit, position)
  position = Math.max(0, position)
  return position
}

// Advance a scroll position by a number of pixels while remaining within
// the scroll limits
function advanceScrollPos(position, amount, contentSize, viewSize)
{
  return clampScrollPos(position + amount/contentSize, contentSize, viewSize)
}

// Advance the scroll position of a scroll bar by the default amount in a
// specified direction.
// contentSize is the scroll view's contentWidth/contentHeight, and viewSize is
// its width/height.
function scroll(direction, scrollBar, contentSize, viewSize) {
  // Default scroll amount in logical pixels
  var scrollAmount = 30

  var amount = direction * scrollAmount
  var newPos = advanceScrollPos(scrollBar.position, amount, contentSize, viewSize)
  scrollBar.position = newPos
}

// Scroll a scroll view horizontally or vertically in a specified direction
function horzScroll(direction, scrollView, horzScrollBar) {
  scroll(direction, horzScrollBar, scrollView.contentWidth, scrollView.width)
}
function vertScroll(direction, scrollView, vertScrollBar) {
  scroll(direction, vertScrollBar, scrollView.contentHeight, scrollView.height)
}

// Check for and handle a vertical or horizontal scroll event in a scroll view.
// If the event is a scroll event in the appropriate direction, accepts it,
// scrolls, reveals the focus cue, and returns true.
function handleVertScrollKeyEvent(keyEvent, scrollView, vertScrollBar, focusCue) {
  if(keyEvent.key === Qt.Key_Up)
    vertScroll(-1, scrollView, vertScrollBar)
  else if(keyEvent.key === Qt.Key_Down)
    vertScroll(1, scrollView, vertScrollBar)
  else
    return false  // Not a vert scroll key

  // Handled the event
  keyEvent.accepted = true
  focusCue.reveal()
  return true
}
function handleHorzScrollKeyEvent(keyEvent, scrollView, horzScrollBar, focusCue) {
  if(keyEvent.key === Qt.Key_Left)
    horzScroll(-1, scrollView, horzScrollBar)
  else if(keyEvent.key === Qt.Key_Right)
    horzScroll(1, scrollView, horzScrollBar)
  else
    return false  // Not a horz scroll key

  // Handled the event
  keyEvent.accepted = true
  focusCue.reveal()
  return true
}

// Handle a scroll event in either direction.
function handleScrollKeyEvent(keyEvent, scrollView, horzScrollBar,
                              vertScrollBar, focusCue) {
  return handleHorzScrollKeyEvent(keyEvent, scrollView, horzScrollBar, focusCue) ||
         handleVertScrollKeyEvent(keyEvent, scrollView, vertScrollBar, focusCue)
}
