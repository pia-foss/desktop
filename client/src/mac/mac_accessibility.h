// Copyright (c) 2019 London Trust Media Incorporated
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

#include "common.h"
#line HEADER_FILE("mac/mac_accessibility.h")

#ifndef MAC_ACCESSIBILITY_H
#define MAC_ACCESSIBILITY_H

#include <QAccessibleInterface>

// The QAccessible implementation on Mac has some very serious limitations that
// prevent it from being usable as-is.  Many state fields and events are simply
// ignored, despite the fact that VoiceOver really needs these events to handle
// all transitions correctly.  There are also many Mac accessibility properties
// that are simply not implemented, despite the relevant information being
// present in QAccessibleInterface (or one of the role-specific interfaces).
//
// In particular:
// - It doesn't forward ObjectCreated and ObjectDestroyed.  This is a major
//   oversight, these are critical to ensure that destroying the control that
//   the VoiceOver cursor is on works correctly.
// - It doesn't implement any table properties, which are critical to get tables
//   to work under VoiceOver.
// - Several roles are not implemented, others simply map to the StaticText and
//   Group roles, even when more specific NSAccessible roles would make more
//   sense.
// - The scroll bar orientation property is not implemented; all scroll bars
//   appear to be horizontal.  (Trivial to implement, but very annoying.)
//
// Some of these features are critical to get VoiceOver to work with the app,
// so we resort to poking some of the internals in order to fill them in.
// Fortunately, the dynamic nature of Objective-C lets us do this at runtime.

// Initialize the Mac accessibility helpers
void macAccInit();

void macPostAccCreated(QAccessibleInterface &element);
void macPostAccDestroyed(QAccessibleInterface &element);

#endif
