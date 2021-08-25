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
#line SOURCE_FILE("mac/mac_accessibility.mm")

#include "mac_accessibility.h"
#include "mac_accessibility_decorator.h"

#include <QtCore/qglobal.h>
#include <AppKit/NSAccessibility.h>
#include <QAccessible>

void macAccInit()
{
    // Init the accessibility element subclass
    macInitElementSubclass();
    macInitPanelSubclass();
}

void macSubclassInterfaceElement(QAccessibleInterface &element)
{
    // Get the Mac element - this creates it if it doesn't exist yet
    QMacAccessibilityElement *pMacElement = macGetAccElement(&element);

    // It's a problem if this somehow fails; we won't be able to fix it, various
    // features won't work.
    if(!pMacElement)
    {
        qWarning() << "Can't subclass element" << &element
            << "- interface couldn't be created";
        return;
    }

    macSubclassElement(pMacElement);
}

void macPostAccCreated(QAccessibleInterface &element)
{
    // If the Mac element hasn't been subclassed yet, subclass it.
    //
    // There isn't really a better place to do this.
    // - We can't trap the creation of the QMacElement.
    // - We can't do it when we're creating the QAccessibleInterface object
    //   (either in its constructor or in the interface factory), because we
    //   have to do it after the interface is registered with QAccessibleCache.
    //
    // We do know though that a create event won't occur until after the
    // interface is created and registered (even if that happens as part of
    // handling the create event, since it needs an interface).  We also know
    // that this will happen before the element could be observed by the screen
    // reader, because AccessibleItem won't report parents/children that haven't
    // been created yet.
    macSubclassInterfaceElement(element);
    macPostAccNotification(element, NSAccessibilityCreatedNotification);
}

void macPostAccDestroyed(QAccessibleInterface &element)
{
    macPostAccNotification(element, NSAccessibilityUIElementDestroyedNotification);
}
