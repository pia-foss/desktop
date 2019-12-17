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
#line HEADER_FILE("controls.h")

#ifndef NATIVEACC_CONTROLS_H
#define NATIVEACC_CONTROLS_H

#include <QQuickItem>
#include "accessibleitem.h"
#include "popupitem.h"
#include "accutil.h"

namespace NativeAcc {

// Several of these types are just AccessibleItems with a role and no other
// properties.
//
// It's still preferable to define these each as distinct attached types, rather
// than actually define roles from QML, because it makes the QML code clearer
// and more concise.
//
// Unfortunately, the moc still requires a lot of this to be spelled out, we
// can't factor it down any further due to QML registered types requiring
// Q_OBJECT.

// Text and Label are used for static text items.  They have the StaticText
// accessibility role.
//
// These types of elements are similar, but not identical:
// - Text is used for "free text" statics, like messages or explanations.  These
//   aren't associated with a neighboring control.  (This annotation appears on
//   all platforms.)
// - Label is used for labels associated with a neighboring control, when that
//   control's title is set to the same text as a label.
//   - On Mac OS only, these elements are hidden.  VoiceOver already reads the
//     control titles, and having these elements also makes navigation somewhat
//     more cumbersome.  This also improves navigation on the Connection page
//     and the IP/Usage tiles; these use column layouts that VoiceOver doesn't
//     read in the right order.
//   - Other platforms do not reliably read control titles, but statics are
//     skipped by default since Narrator/NVDA/Orca all use Tab navigation as
//     their "usual" navigation style, and statics are not tab stops.
//
// Subtle as this is, it's still important to use the correct control type in
// the correct context.
//
// Note that neither of these types are used for "indicator" texts that change
// often (such as he IP address displays, download speed displays, etc.) - those
// should use TextField instead (usually via the ValueText/ValueHtml common
// type.)
//
// Typically, for a static text, the name is the displayed text.  Static texts
// don't normally need a description.
//
// NOTE: StaticText elements report as "editable text" in Windows with Qt
// 5.11.1; fixed in 5.11.2.  https://bugreports.qt.io/browse/QTBUG-69894
class LabelAttached : public AccessibleItem
{
    Q_OBJECT
public:
    LabelAttached(QQuickItem &item);
};
class TextAttached : public AccessibleItem
{
    Q_OBJECT
public:
    TextAttached(QQuickItem &item)
        : AccessibleItem{QAccessible::Role::StaticText, item}
    {}
};

// Group is used for a generic control group that has no other functionality.
// It has the Grouping accessibility role.
//
// These have no other properties, they normally just have a name.
class GroupAttached : public AccessibleItem
{
    Q_OBJECT
public:
    GroupAttached(QQuickItem &item)
        : AccessibleItem{QAccessible::Role::Grouping, item}
    {}
};

// Graphic is used to describe static images.
class GraphicAttached : public AccessibleItem
{
    Q_OBJECT
public:
    GraphicAttached(QQuickItem &item)
        : AccessibleItem{QAccessible::Role::Graphic, item}
    {}
};

// Chart describes a chart graphic.  These usually provide custom accessibility
// hints for the items within the chart and act as a group.
class ChartAttached : public AccessibleItem
{
    Q_OBJECT
public:
    ChartAttached(QQuickItem &item)
        : AccessibleItem{QAccessible::Role::Chart, item}
    {}
};

// Tab lists contain tabs; it just acts like a group otherwise.
class TabListAttached : public AccessibleItem
{
    Q_OBJECT
public:
    TabListAttached(QQuickItem &item)
        : AccessibleItem{QAccessible::Role::PageTabList, item}
    {}
};

// Popup menu - acts like a modal group.  Generates start/end events for the
// menu based on its visibility.
class PopupMenuAttached : public PopupItem
{
    Q_OBJECT

public:
    PopupMenuAttached(QQuickItem &item)
        : PopupItem{QAccessible::Role::PopupMenu, item,
                    QAccessible::Event::PopupMenuStart,
                    QAccessible::Event::PopupMenuEnd}
    {}
};

// Dialog - acts like a modal group.
class DialogAttached : public AccessibleItem
{
    Q_OBJECT

public:
    DialogAttached(QQuickItem &item);
};

}

NATIVEACC_ATTACHED_PROPERTY_STUB(Label, LabelAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(Text, TextAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(Group, GroupAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(Graphic, GraphicAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(Chart, ChartAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(TabList, TabListAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(PopupMenu, PopupMenuAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(Dialog, DialogAttached)

#endif
