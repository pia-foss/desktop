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
#line SOURCE_FILE("controls.cpp")

#include "controls.h"

namespace NativeAcc {

LabelAttached::LabelAttached(QQuickItem &item)
    : AccessibleItem{QAccessible::Role::StaticText, item}
{
#ifdef Q_OS_MACOS
    // Suppress this element completely on Mac OS per class description.
    setHidden(true);
#endif
}

// We don't use the Dialog type, because on Mac, QAccessible always ignores this
// type.
// (It maps Dialog to the native Window type, and then ignores all Window types
// since it assumes they correspond to native Windows, which have a native
// accessibility hint.)
DialogAttached::DialogAttached(QQuickItem &item)
    : AccessibleItem{QAccessible::Role::Grouping, item}
{
    setState(StateField::modal, true);
}

}
