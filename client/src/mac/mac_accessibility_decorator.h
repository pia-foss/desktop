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

#include "common.h"
#line HEADER_FILE("mac/mac_accessibility_decorator.h")

#ifndef MAC_ACCESSIBILITY_DECORATOR_H
#define MAC_ACCESSIBILITY_DECORATOR_H

#include <QAccessible>
#include <QAccessibleInterface>
#include <QtCore/qglobal.h>

#import <AppKit/AppKit.h>
#import <AppKit/NSAccessibility.h>

// As discussed in the other mac_accessibility headers, Qt's accessibility
// implementation on Mac has some serious gaps that need to be filled in.
//
// This class decorates a QMacAccessibilityElement to fill in some of those
// gaps.  We can't control when the accessibility elements are created, but we
// do know that an ObjectCreated event is generated for them before they're
// observed by the screen reader (because we generate it; Qt doesn't).
//
// When the ObjectCreated event is being generated, we subclass the
// QMacAccessibilityElement so we can override some of its methods.

// Forward-declare QMacAccessibilityElement.
@class QMacAccessibilityElement;

void macInitElementSubclass();
void macInitPanelSubclass();

// Get the QMacAccessibilityElement for a given ID, including creating it if it
// doesn't yet exist.
// (This is a class method of QMacAccessibilityElement).
QMacAccessibilityElement *macElementForId(QAccessible::Id accId);

// Get the QMacAccessibilityElement for a given QAccessibleInterface, including
// creating it if it doesn't yet exist.
// (Gets the ID and calls macElementForId().)
QMacAccessibilityElement *macGetAccElement(QAccessibleInterface *pElement);

// Manually post a Mac accessibility notification for an accessible element.
void macPostAccNotification(QAccessibleInterface &element,
                            NSAccessibilityNotificationName event);

// Subclass a QMacAccessibilityElement.  Assigns it a new class that's
// dynamically built to subclass QMacAccessibilityElement.
void macSubclassElement(QMacAccessibilityElement *element);

#endif
