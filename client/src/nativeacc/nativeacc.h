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

#include <common/src/common.h>
#line HEADER_FILE("nativeacc.h")

#ifndef NATIVEACC_NATIVEACC_H
#define NATIVEACC_NATIVEACC_H

#include <QQuickItem>

/*

=== NativeAcc ===

The classes in the NativeAcc namespace hook up QtQuick Items to QAccessible
accessibility annotations.

QtQuick does provide the QML Accessible type to do exactly that, but it has a
number of problems and missing features that prevent it from being usable.

--- Pre-annotated Items ---

Several pre-annotated items are provided in the common/ directory that have
accessibility annotations and require minimal extra work.

Mainly, these are:
 - StaticText - for static text labels, a Text with annotations
 - ValueText - for displaying text values, a Text with annotations and 'label' property
 - ValueHtml - for displaying HTML text values, a Text with annotations and 'label' property
 - ButtonArea - like MouseArea tuned for push buttons, has 'name' and 'description' properties
 - CheckButtonArea - like MouseArea tuned for toggle buttons, has 'name', 'description', and 'checked'
 - TextLink - a clickable text hyperlink

These also provide keyboard navigation features when appropriate.

--- Annotating an Item ---

Custom Items will need to be annotated with accessibility annotations.  There
are a few steps for this:
1. Import NativeAcc: `import PIA.NativeAcc 1.0 as NativeAcc`
2. Choose the NativeAcc type that best models the control, like TextField,
   Button, or Group.
3. Attach the NativeAcc type to the Item and set the appropriate properties,
   like:
      Item {
        ...
        NativeAcc.Button.name: <...button's name...>
        NativeAcc.Button.description: <...description, if needed...>
      }

Usually, the item that receives keyboard focus for keyboard navigation should be
annotated, if there is one.  Some properties of NativeAcc types (disabled,
visible, etc.) are set automatically from the Item's state, and usually the
keyboard-focused item has the correct properties for this.

--- Adding a new NativeAcc type ---

If a new NativeAcc type is needed, such as for a new role (or even for a
different style of model for an existing role), it can be implemented with a few
steps (Label is a simple example to follow along with):
1. Define a new attaching type derived from AccessibleItem.  For example, to
   define a NativeAcc.PushButton type, create a class called PushButtonAttached
   derived from AccessibleItem.
2. Add `NATIVEACC_ATTACHED_PROPERTY_STUB(<type>, <type>Attached)` at global
   scope in the header to declare the attached property stub type.
3. Register the type with QML in NativeAcc::init() (nativeacc.cpp)
4. The class's constructor should take a QQuickItem& and specify the class's
   role when constructing AccessibleItem.
5. If necessary, add additional properties to the *Attached type for additional
   info needed from QML.
6. If necessary, implement additional QAccessible interfaces like
   QAccessibleTextInterface, QAccessibleActionInterface, etc.  Implement the
   AccessibleItem interface accessor methods for each needed interface.

Often it will also make sense to create a new QML type in common/ with this
annotation for the most common usage patterns, too.

--- Modal overlays ---

NativeAcc supports modal overlays that parent to Overlay.overlay (which Popup
and derived types do).

When a modal overlay is active and has accessibility annotations, WindowAccImpl
switches from the non-overlay content to the overlay content.  The user cannot
return to the non-overlay content until the overlay is closed (just like a
sighted user navigating with the mouse/keyboard).

NativeAcc assumes that any overlay with annotations is modal.  Modeless overlays
can be used if they do not have annotations, such as InfoTip, which annotates
the non-overlay item with the text that would be displayed in the overlay.

Modeless overlays could probably be made to work with a proper Group annotation
that indicates that the overlay is modeless, but the client avoids these anyway
since they are pretty complex UX.

--- Limitations ---

QML Accessible annotations can't be mixed with NativeAcc annotations.  The issue
here is that QML Accessible only includes other QML Accessible annotations in
its parent/child relationships - custom annotations can't be made part of the
tree.

NativeAcc only includes NativeAcc annotations similarly due to that limitation.
Most of the QQuickControls 2 controls have QML Accessible annotations already,
but they're pretty bad most of the time, so it's actually beneficial that
they're excluded.

We can override QML Accessible's interface factories by defining ours last, but
we can't remove them entirely.  If an object has built-in Acessible annotations
(most controls do), NativeAcc annotations can usually override it as long as
they are defined declaratively.  Imperatively-assigned NativeAcc annotations
only work as long as the object does not have Accessible annotations, since
Accessible will have a chance to create an interface before the NativeAcc
annotation is attached.

This doesn't always work though and is sometimes platform-dependent - for
example, QQuickCheckBox can't be overridden on Linux; it creates its
accessibility element before even declarative property bindings are evaluated.
(This is platform dependent because it depends on when the platform plugin
enables accessibility.)

For now this has been worked around in individual controls (annotate a wrapper
rather than the control itself).  It'd be more robust in the future to always
return a stub from the interface factory and dynamically hook that up to the
NativeAcc annotation if one is created later though.

--- QML Accessible issues ---

For the sake of posterity, below are the issues encountered when attempting to
use the QML Accessible type.

Broadly, there are a lot of roles and flags in QAccessible that simply are not
supported - critical features such as tables, the 'disabled' flag, modals, etc.

The lack of Table type support, together with the fact that QML Accessible and
NativeAcc can't be mixed, is enough to render QML Accessible completely
unusable.  Some of the other problems could be worked around (we could put
"disabled" as text into button names, for example), but the workarounds are
pretty bad (the tool can't actually tell the button state, can't split up the
name and "disabled", we have to localize "disabled" ourselves, etc.)

Menus and dialogs are also really bad.  In both cases, the item should be modal,
the user shouldn't be allowed to escape to the rest of the window.  Menus can be
made to somewhat work - the user can still escape, but as long as keyboard focus
follows screen reader focus, the menu closes.  This works, but is confusing if
the user can't actually see what's going on.  Dialogs are completely unusable;
there's no way to annotate the dialog's root item (it's not the contentItem),
and the dialog controls are intermixed with the regular controls underneath
them.

Accessible is also pretty hard to use and poorly documented - it attempts to
implement all roles with one attached type, and various roles respect various
subsets of its properties in undocumented/inconsistent ways.  NativeAcc models
separate roles with separate attached types, in some cases there are even
multiple models for the same role used in different contexts.

- Disabled controls aren’t reported
- No support for Table type at all
- Coords don’t apply scale to size
- Settings overlay dialogs - dialog should be modal, can’t annotate dialog's root item
- Header menu: separators are interactive
- Header menu: Esc to close still acts like menu is visible
- Links - link type does not work from QML
- Links - can’t read URL
- Buttons - duplicate “press” action in actions list
- Everything - visibility changes seemingly aren’t notified to VoiceOver
- Tiles - favorite/unfavorite causes acc hints to disappear or break (quick settings, quick connect, region)
- Info tip - only static text type works, better types don’t work
- Perf module - indicators really need labels, can't put labels on text from QML (also Usage, IP modules)
- Perf module - chart bars say they’re groups for some reason, should be indicators
- Settings tabs - buttons don’t list positions (“2 of 6”)
- Settings tabs - buttons don’t list press/select action (generates simulated mouse click somehow)
- Drop down input - doesn’t list action to show menu; pop up does work but says it triggers the action
- Drop down input - doesn’t have label
- Connection page - Controls desperately need labels due to poor control order (also Usage module)-
- Settings overlay dialogs - dialog should be modal, can’t annotate dialog itself (popup item?)

*/

namespace NativeAcc {

// Initialize NativeAcc.
// - Installs QAccessible factories.
// - Registers all types in the NativeAcc module with the QML engine.
void init();

}

#endif
