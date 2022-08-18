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

#include <common/src/common.h>
#line HEADER_FILE("popupitem.h")

#ifndef NATIVEACC_POPUPITEM_H
#define NATIVEACC_POPUPITEM_H

#include "accessibleitem.h"
#include "accutil.h"

namespace NativeAcc {

// PopupItem is any item that acts as a modal overlay, such as a popup menu or
// overlay dialog.
// It generates start/end events when the overlay is shown or hidden.
class PopupItem : public AccessibleItem
{
    Q_OBJECT

public:
    // startEvent/endEvent are the events sent when the popup appears or
    // disappears.
    PopupItem(QAccessible::Role popupRole, QQuickItem &item,
              QAccessible::Event startEvent, QAccessible::Event endEvent);

private:
    void onStateChanged(StateField field);

private:
    QAccessible::Event _startEvent, _endEvent;
};

}

#endif
