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
#line SOURCE_FILE("mac/mac_accessibility_decorator.mm")

#include "mac_accessibility_decorator.h"
#include "nativeacc/interfaces.h"
#import <AppKit/AppKit.h>
#include <objc/runtime.h>

// Uses the deprecated NSAccessibility "informal protocol" because that's what
// Qt uses.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Get the base class's implementation of a method
template<class FuncT>
FuncT getBaseMethod(Class baseClass, SEL selector)
{
    Method baseMethod = class_getInstanceMethod(baseClass, selector);
    Q_ASSERT(baseMethod);   // Class must have this method

    FuncT baseImpl = reinterpret_cast<FuncT>(method_getImplementation(baseMethod));
    Q_ASSERT(baseImpl); // Methods always have a valid implementation
    return baseImpl;
}

// Create a method on subclass corresponding to a method of baseClass, and
// return baseClass's implementation.
template<class FuncT>
FuncT subclassMethod(Class subclass, Class baseClass, SEL selector,
                     FuncT implFunc, const char *funcTypes)
{
    FuncT baseImpl = getBaseMethod<FuncT>(baseClass, selector);
    class_addMethod(subclass, selector, reinterpret_cast<IMP>(implFunc),
                    funcTypes);
    return baseImpl;
}

// Add a method on subclass that is not implemented in baseClass.  (Verifies
// that the method is actuall not implemented in the base class - use
// subclassMethod() instead if it is, as the base implementation should usually
// be used.)
template<class FuncT>
void addMethod(Class subclass, Class baseClass, SEL selector,
               FuncT implFunc, const char *funcTypes)
{
    Method baseMethod = class_getInstanceMethod(baseClass, selector);
    Q_ASSERT(!baseMethod);   // Class must _not_ have this method
    class_addMethod(subclass, selector, reinterpret_cast<IMP>(implFunc),
                    funcTypes);
}

/*** Base methods of QMacAccessibilityElement ***/
// Initialized when the subclass is built.

// The base class itself.
Class macElementBaseClass = nil;
// Offset to the axid member of a QMacAccessibilityElement
std::ptrdiff_t axidOffset = 0;

// Class method to get an element for a QAccessible::Id.
// The logic to create the element if it doesn't exist yet is in this method.
using ElementWithIdFunc = QMacAccessibilityElement*(*)(id, SEL, QAccessible::Id);
ElementWithIdFunc classElementWithIdFunc = nil;

using AccessibilityRoleFunc = NSAccessibilityRole(*)(id, SEL);
AccessibilityRoleFunc baseAccessibilityRole = nil;

using AccessibilityValueFunc = id(*)(id, SEL);
AccessibilityValueFunc baseAccessibilityValue = nil;

using AccessibilityChildrenFunc = id(*)(id, SEL);
AccessibilityChildrenFunc baseAccessibilityChildren = nil;

using AccessibilityParameterizedAttributeNamesFunc = NSArray<NSString*>*(*)(id, SEL);
AccessibilityParameterizedAttributeNamesFunc baseAccessibilityParameterizedAttributeNames = nil;

using AccessibilityAttributeValueForParameterFunc = id(*)(id, SEL, NSString*, id);
AccessibilityAttributeValueForParameterFunc baseAccessibilityAttributeValueForParameter = nil;

using AccessibilityActionNamesFunc = NSArray<NSAccessibilityActionName>*(*)(id, SEL);
AccessibilityActionNamesFunc baseAccessibilityActionNames = nil;

using AccessibilityActionDescriptionFunc = NSString*(*)(id, SEL, NSAccessibilityActionName);
AccessibilityActionDescriptionFunc baseAccessibilityActionDescription = nil;

using AccessibilityPerformActionFunc = void(*)(id, SEL, NSAccessibilityActionName);
AccessibilityPerformActionFunc baseAccessibilityPerformAction = nil;

/*** Helper methods for the subclass ***/

// Get the QAccessibleInterface corresponding to a QMacAccessibilityElement.
// Returns nullptr if there isn't one or self is nullptr.
QAccessibleInterface *getAccInterface(id self)
{
    if(!self)
        return nullptr;

    // Do this with the member offset in axidOffset.  We can't use
    // object_getIvar() because the value is not an object pointer.
    // Calculating an offset from an id also has an extra step under ARC to get
    // away from a retainable pointer.
    void *selfVoid = (__bridge void *)self;
    unsigned char *pAxidCalc = reinterpret_cast<unsigned char *>(selfVoid);
    pAxidCalc += axidOffset;
    QAccessible::Id *pAxid = reinterpret_cast<QAccessible::Id *>(pAxidCalc);

    return QAccessible::accessibleInterface(*pAxid);
}

// Get various role interfaces.
NativeAcc::AccessibleElement *getAccElement(id self)
{
    QAccessibleInterface *pAccItf = getAccInterface(self);
    return dynamic_cast<NativeAcc::AccessibleElement*>(pAccItf);
}

template<class AccItfT>
AccItfT *getAccItf(id self, AccItfT *(QAccessibleInterface::*pItfMethod)())
{
    QAccessibleInterface *pAccItf = getAccInterface(self);
    return pAccItf ? (pAccItf->*pItfMethod)() : nullptr;
}
auto getAction(id self) {return getAccItf(self, &QAccessibleInterface::actionInterface);}
auto getTable(id self) {return getAccItf(self, &QAccessibleInterface::tableInterface);}
auto getTableCell(id self) {return getAccItf(self, &QAccessibleInterface::tableCellInterface);}

template<class FillerItfT>
FillerItfT *getFillerItf(id self, FillerItfT *(NativeAcc::AccessibleElement::*pItfMethod)())
{
    QAccessibleInterface *pAccItf = getAccInterface(self);
    NativeAcc::AccessibleElement *pAccElement = dynamic_cast<NativeAcc::AccessibleElement*>(pAccItf);
    return pAccElement ? (pAccElement->*pItfMethod)() : nullptr;
}
auto getTableFiller(id self) {return getFillerItf(self, &NativeAcc::AccessibleElement::tableFillerInterface);}
auto getRowFiller(id self) {return getFillerItf(self, &NativeAcc::AccessibleElement::rowFillerInterface);}

// Return a list of accessibility elements as an NSArray of Mac elements.
NSArray<QMacAccessibilityElement*> *returnMacElements(const QList<QAccessibleInterface*> &elements)
{
    NSMutableArray<QMacAccessibilityElement*> *macElements =
        [NSMutableArray<QMacAccessibilityElement*> arrayWithCapacity:elements.size()];

    for(const auto &pElement : elements)
        [macElements addObject:macGetAccElement(pElement)];

    return macElements;
}

// Return an empty list of accessibility elements (a relatively verbose function
// call otherwise)
NSArray<QMacAccessibilityElement*> *emptyMacElements()
{
    return [NSArray<QMacAccessibilityElement*> array];
}

/*** Replacement methods for the subclass ***/

// The subclass itself.
Class macElementSubclass = nil;

/*** Attribute accessor overrides ***/

NSArray *accessibilityChildrenInNavigationOrder(id self, SEL _cmd)
{
    // We already return children in the proper navigation order, and we really
    // want VoiceOver to use that order.  This is important for two-column
    // layouts on Settings pages in particular.
    return baseAccessibilityChildren(self, _cmd);
}

/*** Role/subrole fixups ***/

NSAccessibilityRole accessibilityRole(id self, SEL _cmd)
{
    Q_ASSERT(baseAccessibilityRole);    // Initialized

    QAccessibleInterface *pAccItf = getAccInterface(self);

    // Use the "pop-up button" role for combo boxes.  The "ButtonDropDown" type
    // would really be a little more appropriate, but this type doesn't read
    // well on Windows (it just says it's a "button", drop downs are normally
    // annotated as combo boxes).
    if(pAccItf && pAccItf->role() == QAccessible::Role::ComboBox)
        return NSAccessibilityPopUpButtonRole;

    // For tables, substitute the "outline" row, because our tables can have
    // collapsed sections.
    // This doesn't require every table to have collapsed sections, it's
    // common on Mac to use the outline role for plain tables too.
    // (We only have one table right now anyway, the regions list.)
    if(pAccItf && pAccItf->role() == QAccessible::Role::Table)
        return NSAccessibilityOutlineRole;

    return baseAccessibilityRole(self, _cmd);
}

NSAccessibilitySubrole accessibilitySubrole(id self, SEL)
{
    QAccessibleInterface *pAccItf = getAccInterface(self);

    // Like tables above, use the outline-row subrole for table rows.
    if(pAccItf && pAccItf->role() == QAccessible::Role::Row)
        return NSAccessibilityOutlineRowSubrole;

    return NSAccessibilityUnknownSubrole;
}

/*** Table attributes ***/
NSArray *accessibilityColumns(id self, SEL)
{
    NativeAcc::AccessibleTableFiller *pTableItf = getTableFiller(self);
    return pTableItf ? returnMacElements(pTableItf->getTableColumns()) : emptyMacElements();
}

NSArray *accessibilityVisibleColumns(id self, SEL)
{
    NativeAcc::AccessibleTableFiller *pTableItf = getTableFiller(self);
    return pTableItf ? returnMacElements(pTableItf->getTableColumns()) : emptyMacElements();
}

NSArray *accessibilityRows(id self, SEL)
{
    NativeAcc::AccessibleTableFiller *pTableItf = getTableFiller(self);
    return pTableItf ? returnMacElements(pTableItf->getTableRows()) : emptyMacElements();
}

NSArray *accessibilityVisibleRows(id self, SEL)
{
    NativeAcc::AccessibleTableFiller *pTableItf = getTableFiller(self);
    return pTableItf ? returnMacElements(pTableItf->getTableRows()) : emptyMacElements();
}

NSArray *accessibilitySelectedColumns(id, SEL)
{
    return emptyMacElements();
}

NSArray *accessibilitySelectedRows(id self, SEL)
{
    NativeAcc::AccessibleTableFiller *pTableItf = getTableFiller(self);
    return pTableItf ? returnMacElements(pTableItf->getSelectedRows()) : emptyMacElements();
}

/*** Table row attributes ***/

BOOL isAccessibilityDisclosed(id self, SEL)
{
    // "disclosing" means "is the row disclosing its children"; i.e. "is it
    // expanded"
    NativeAcc::AccessibleRowFiller *pRowItf = getRowFiller(self);
    return pRowItf ? pRowItf->getExpanded() : false;
}

id accessibilityDisclosedRows(id self, SEL)
{
    NativeAcc::AccessibleRowFiller *pRowItf = getRowFiller(self);
    return pRowItf ? returnMacElements(pRowItf->getOutlineChildren()) : emptyMacElements();
}

id accessibilityDisclosedByRow(id self, SEL)
{
    NativeAcc::AccessibleRowFiller *pRowItf = getRowFiller(self);
    return pRowItf ? macGetAccElement(pRowItf->getOutlineParent()) : nil;
}

NSInteger accessibilityDisclosureLevel(id self, SEL)
{
    // "disclosure level" is the "outline level" - number of indentations
    NativeAcc::AccessibleRowFiller *pRowItf = getRowFiller(self);
    return pRowItf ? pRowItf->getOutlineLevel() : 0;
}

NSInteger accessibilityIndex(id self, SEL)
{
    QAccessibleTableCellInterface *pCellItf = getTableCell(self);
    return pCellItf ? pCellItf->rowIndex() : 0;
}

/*** Table cell attributes ***/

NSRange accessibilityColumnIndexRange(id self, SEL)
{
    QAccessibleTableCellInterface *pCellItf = getTableCell(self);
    NSRange range{};
    if(pCellItf)
    {
        range.location = pCellItf->columnIndex();
        range.length = pCellItf->columnExtent();
    }
    return range;
}

NSRange accessibilityRowIndexRange(id self, SEL)
{
    QAccessibleTableCellInterface *pCellItf = getTableCell(self);
    NSRange range{};
    if(pCellItf)
    {
        range.location = pCellItf->rowIndex();
        range.length = pCellItf->rowExtent();
    }
    return range;
}

/*** General attribute fixups ***/

// Qt incorrectly implements an "accessibilityEnabledAttribute" getter, it's
// actually "isAccessibilityEnabled".
// This means "is the control represented by the accessibility element enabled",
// not whether accessibility is enabled at all for this control.
BOOL isAccessibilityEnabled(id self, SEL)
{
    QAccessibleInterface *pAccItf = getAccInterface(self);
    return pAccItf ? !pAccItf->state().disabled : true;
}

id accessibilityValue(id self, SEL _cmd)
{
    Q_ASSERT(baseAccessibilityValue);

    // The 'value' implementation in QAccessible is especially bizarre.  Fix
    // it up on Mac to behave like Win/Linux.
    //
    // All platforms have a generic "value" concept.  Windows also has the
    // concept of a "range value", which is numeric with min/max.  On Mac,
    // this is folded into the generic "value", which is a variant value
    // with optional min/max variants.
    //
    // The bizarre part is how Qt implements these concepts.  Qt has both a
    // "text value" concept (QAccessible::Text::Value) and a "variant value"
    // concept (QAccessibleValueInterface).  It seems like these would fit
    // the platform models pretty clearly.
    //
    // On Mac though, Qt never uses QAccessible::Text::Value at all (as a
    // getter anyway).  It exclusively uses QAccessibleValueInterface's
    // value as a string, with some "magic" sprinkled in to fix up some
    // specific roles (getValueAttribute() in qcocoaaccessibility.mm).
    //
    // On Windows, Qt uses QAccessible::Text::Value for the string value.
    // If the control provides QAccessibleValueInterface, Qt assumes it is
    // numeric and exclusively uses the value as a double.
    //
    // So for a text-valued control like a combo box, it's possible to
    // implement the correct behavior on each platform, but there's no way
    // to implement the correct behavior on _both_ platforms simultaneously.
    //
    // For the affected controls, just return the Value text as the value
    // as Qt probably should have.  Keep Qt's other quirks (for now at
    // least), like mapping StaticTexts' names to their value instead of
    // description.
    QAccessibleInterface *pAccItf = getAccInterface(self);
    QAccessible::Role role = pAccItf ? pAccItf->role() : QAccessible::Role::NoRole;

    // - Fix up the value for combo boxes.
    // - Fix up the value for edits.  Qt would fill it in from the text
    //   interface, but these controls also have to provide the text value
    //   too in order to work on Windows.  For consistency, always use this
    //   value.  (Note that ValueText provides this value but does not have
    //   a text interface, since it is not editable.)
    if(role == QAccessible::Role::ComboBox ||
        role == QAccessible::Role::EditableText)
    {
        qInfo() << "returning" << pAccItf->text(QAccessible::Text::Value)
            << "for role" << traceEnum(role);
        return pAccItf->text(QAccessible::Text::Value).toNSString();
    }

    return baseAccessibilityValue(self, _cmd);
}

NSArray<NSString *> *accessibilityParameterizedAttributeNames(id self, SEL _cmd)
{
    Q_ASSERT(baseAccessibilityParameterizedAttributeNames);
    static NSArray<NSString*> *tableAttributes = @[
        NSAccessibilityCellForColumnAndRowParameterizedAttribute
    ];

    NSArray<NSString*> *baseAttrs = baseAccessibilityParameterizedAttributeNames(self, _cmd);

    // If this has the table and table filler interfaces, add the table attributes
    if(getTableFiller(self))
        return [baseAttrs arrayByAddingObjectsFromArray:tableAttributes];

    return baseAttrs;
}

id accessibilityAttributeValueForParameter(id self, SEL _cmd, NSString *attribute, id parameter)
{
    Q_ASSERT(baseAccessibilityAttributeValueForParameter);

    if([attribute isEqualToString:NSAccessibilityCellForColumnAndRowParameterizedAttribute])
    {
        QAccessibleInterface *pAccItf = getAccInterface(self);
        QAccessibleTableInterface *pTable = pAccItf ? pAccItf->tableInterface() : nullptr;

        if(pTable)
        {
            NSArray<NSNumber*> *coords = (NSArray<NSNumber*>*)parameter;
            unsigned column = [coords[0] unsignedIntValue];
            unsigned row = [coords[1] unsignedIntValue];

            QAccessibleInterface *pCell = pTable->cellAt(row, column);
            return macGetAccElement(pCell);
        }

        return nil;
    }

    return baseAccessibilityAttributeValueForParameter(self, _cmd, attribute, parameter);
}

bool isQtMappedAction(const QString &qtAction)
{
    return qtAction == QAccessibleActionInterface::pressAction() ||
            qtAction == QAccessibleActionInterface::increaseAction() ||
            qtAction == QAccessibleActionInterface::decreaseAction() ||
            qtAction == QAccessibleActionInterface::showMenuAction() ||
            qtAction == QAccessibleActionInterface::setFocusAction() ||
            qtAction == QAccessibleActionInterface::toggleAction();
}

bool isQtMappedAction(NSString *macAction)
{
    if(!macAction)
        return false;
    // For whatever reason, Qt uses compare instead of isEqualToString, do the
    // same here.
    // There is one fewer Mac action than Qt actions because press and toggle
    // are both mapped to the Mac Press action.  (Qt then checks the role for
    // the reverse mapping and assumes the original would have been press or
    // toggle, which isn't always right, but we're not fixing that.)
    return [macAction compare:NSAccessibilityPressAction] == NSOrderedSame ||
            [macAction compare:NSAccessibilityIncrementAction] == NSOrderedSame ||
            [macAction compare:NSAccessibilityDecrementAction] == NSOrderedSame ||
            [macAction compare:NSAccessibilityShowMenuAction] == NSOrderedSame ||
            [macAction compare:NSAccessibilityRaiseAction] == NSOrderedSame;
}

NSArray<NSAccessibilityActionName> *accessibilityActionNames(id self, SEL _cmd)
{
    Q_ASSERT(baseAccessibilityActionNames);

    NSArray<NSAccessibilityActionName> *baseActions = baseAccessibilityActionNames(self, _cmd);

    QAccessibleActionInterface *pAccAction = getAction(self);

    if(!pAccAction)
        return baseActions;

    // Qt completely ignores custom actions on Mac.  Honestly, this seems to
    // just be a mistake, because it does have code to pass custom actions
    // through to get action descriptions.
    //
    // In principle, we should keep the actions in the original order, since
    // they're supposed to be ordered based on the user's propensity to use each
    // action.
    //
    // However, Qt's mapping from default action names to Mac action names isn't
    // available in the API, so fully maintaining the order is nontrivial.
    // Instead, we just insert all the Qt-mapped actions when we reach the first
    // action that's understood by Qt.
    //
    // This at least preserves the default action, which is very important for
    // VoiceOver, since that's the action VO+Space takes.
    const QStringList &actions = pAccAction->actionNames();

    NSMutableArray<NSAccessibilityActionName> *fixedActions =
        [NSMutableArray<NSAccessibilityActionName> arrayWithCapacity:actions.size()];

    for(const auto &action : actions)
    {
        if(isQtMappedAction(action))
        {
            // Qt would have mapped this action.  Put all the Qt-mapped actions
            // in here (if we haven't taken them already).
            if(baseActions)
            {
                [fixedActions addObjectsFromArray:baseActions];
                baseActions = nil;
            }
        }
        else
        {
            // Qt does not map this action.  Put the action name in as a custom
            // action.
            [fixedActions addObject:action.toNSString()];
        }
    }

    return fixedActions;
}

NSString *accessibilityActionDescription(id self, SEL _cmd, NSAccessibilityActionName action)
{
    Q_ASSERT(baseAccessibilityActionDescription);
    // Qt actually already passes through custom actions as-is to
    // QAccessibleActionInterface::localizedActionDescription(), but I don't
    // trust it since they obviously are not testing it.  (It's not possible to
    // actually reach that control flow normally since accessibilityActionNames
    // ignores custom actions.)
    if(isQtMappedAction(action))
        return baseAccessibilityActionDescription(self, _cmd, action);

    QAccessibleActionInterface *pActionInterface = getAction(self);
    if(pActionInterface)
    {
        QString qtAction = QString::fromNSString(action);
        return pActionInterface->localizedActionDescription(qtAction).toNSString();
    }

    return nil;
}

void doMacPressAction(id self, SEL _cmd)
{
    // Qt handles the Mac Press action incorrectly.  It maps both the Qt
    // Toggle and Press actions to the Mac Press action, but the reverse
    // mapping checks the control's role to guess which action it was
    // supposed to be, instead of checking the actions the control actually
    // supports.
    //
    // Additionally, it assumes that radio buttons use the "toggle" action,
    // which isn't correct because radio buttons can't be freely toggled
    // (they can only be checked).
    //
    // On top of that, the Windows backend assumes that radio buttons have a
    // press action (somewhat correctly).
    QAccessibleActionInterface *pActionInterface = getAction(self);
    if(!pActionInterface)
    {
        baseAccessibilityPerformAction(self, _cmd, NSAccessibilityPressAction);
        return;
    }

    QStringList actions = pActionInterface->actionNames();

    bool supportsPress = actions.contains(QAccessibleActionInterface::pressAction());
    bool supportsToggle = actions.contains(QAccessibleActionInterface::toggleAction());

    // If it supports exactly one of the two actions, do that one.
    if(supportsPress && !supportsToggle)
        pActionInterface->doAction(QAccessibleActionInterface::pressAction());
    else if(supportsToggle && !supportsPress)
        pActionInterface->doAction(QAccessibleActionInterface::toggleAction());
    else
    {
        // It supports both or neither, just do what Qt would have done
        // otherwise for lack of any better option.
        baseAccessibilityPerformAction(self, _cmd, NSAccessibilityPressAction);
    }
}

void accessibilityPerformAction(id self, SEL _cmd, NSAccessibilityActionName action)
{
    Q_ASSERT(baseAccessibilityPerformAction);

    if([action compare:NSAccessibilityPressAction] == NSOrderedSame)
    {
        // Fix up the Press action.
        doMacPressAction(self, _cmd);
    }
    else if(!isQtMappedAction(action))
    {
        // Handle custom actions.
        QAccessibleActionInterface *pActionInterface = getAction(self);
        if(pActionInterface)
            pActionInterface->doAction(QString::fromNSString(action));
    }
    else
    {
        // Let Qt handle the other standard actions.
        baseAccessibilityPerformAction(self, _cmd, action);
    }
}

/*** Base methods of QNSPanel ***/

// The base class itself.
Class macPanelBaseClass = nil;

using AccessibilityAttributeValueFunc = id(*)(id, SEL, NSString*);
AccessibilityAttributeValueFunc panelBaseAccessibilityAttributeValue = nil;

/*** Additional methods added at runtime ***/

NSArray *panelAccessibilityChildrenInNavigationOrder(id self, SEL _cmd)
{
    // Enforce navigation order for QNSPanel too.  This is important so that
    // top-level controls are navigated in the proper order in our windows.
    //
    // We have to do this on QNSPanel, because it's the next non-ignored element
    // in the heirarchy above our top-level controls.  Although there is an
    // element for the "window" associated with our QQuickWindow/WindowAccImpl,
    // it is ignored for accessibility, and that means that the QNSPanel above
    // it provides the children in navigation order.
    //
    // Additionally, QNSPanel still uses the legacy NSAccessibility protocol,
    // which doesn't have a "children in navigation order" attribute.
    // Fortunately, we seem to be able to add on the new-style method and it is
    // used properly.
    return panelBaseAccessibilityAttributeValue(self, _cmd, NSAccessibilityChildrenAttribute);
}

// Create the subclass for QMacAccessibilityElement.
void macInitElementSubclass()
{
    if(!macElementSubclass)
    {
        macElementBaseClass = objc_getClass("QMacAccessibilityElement");
        Q_ASSERT(macElementBaseClass);    // This class has to exist

        macElementSubclass = objc_allocateClassPair(macElementBaseClass, "SubclassedMacAccElement", 0);

        // Get the class method elementWithId:.
        Method elementWithIdMethod = class_getClassMethod(macElementBaseClass, @selector(elementWithId:));
        Q_ASSERT(elementWithIdMethod);  // Has this method
        classElementWithIdFunc = reinterpret_cast<ElementWithIdFunc>(method_getImplementation(elementWithIdMethod));
        Q_ASSERT(classElementWithIdFunc);   // Methods have valid implementations

        // Get the axid instance variable.
        Ivar axidVar = class_getInstanceVariable(macElementBaseClass, "axid");
        Q_ASSERT(axidVar);  // It must have this variable
        axidOffset = ivar_getOffset(axidVar);

        baseAccessibilityRole = subclassMethod(macElementSubclass,
            macElementBaseClass, @selector(accessibilityRole),
            &accessibilityRole, "@@:");

        // The remaining attributes are not implemented by Qt, so they do not
        // have base implementations.
        addMethod(macElementSubclass, macElementBaseClass,
            @selector(accessibilitySubrole), &accessibilitySubrole,
            "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityColumns), &accessibilityColumns,
                  "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityVisibleColumns), &accessibilityVisibleColumns,
                  "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityRows), &accessibilityRows,
                  "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityVisibleRows), &accessibilityVisibleRows,
                  "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilitySelectedColumns), &accessibilitySelectedColumns,
                  "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilitySelectedRows), &accessibilitySelectedRows,
                  "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(isAccessibilityDisclosed), &isAccessibilityDisclosed,
                  "c@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityDisclosedRows), &accessibilityDisclosedRows,
                  "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityDisclosedByRow), &accessibilityDisclosedByRow,
                  "@@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityDisclosureLevel), &accessibilityDisclosureLevel,
                  "q@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityIndex), &accessibilityIndex,
                  "q@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityColumnIndexRange), &accessibilityColumnIndexRange,
                  "{_NSRange=QQ}@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(accessibilityRowIndexRange), &accessibilityRowIndexRange,
                  "{_NSRange=QQ}@:");
        addMethod(macElementSubclass, macElementBaseClass,
                  @selector(isAccessibilityEnabled), &isAccessibilityEnabled,
                  "c@:");
        baseAccessibilityValue = subclassMethod(macElementSubclass,
            macElementBaseClass, @selector(accessibilityValue),
            &accessibilityValue, "@@:");

        // accessibilityChildren isn't overridden, we just need the base
        // method to hook it up to accessibilityChildrenInNavigationOrder
        baseAccessibilityChildren = getBaseMethod<AccessibilityChildrenFunc>(macElementBaseClass,
            @selector(accessibilityChildren));

        addMethod(macElementSubclass, macElementBaseClass,
            @selector(accessibilityChildrenInNavigationOrder),
            &accessibilityChildrenInNavigationOrder, "@@:");

        baseAccessibilityParameterizedAttributeNames = subclassMethod(macElementSubclass,
            macElementBaseClass, @selector(accessibilityParameterizedAttributeNames),
            &accessibilityParameterizedAttributeNames, "@@:");

        baseAccessibilityAttributeValueForParameter = subclassMethod(macElementSubclass,
            macElementBaseClass, @selector(accessibilityAttributeValue:forParameter:),
            &accessibilityAttributeValueForParameter, "@@:@@");

        baseAccessibilityActionNames = subclassMethod(macElementSubclass,
            macElementBaseClass, @selector(accessibilityActionNames),
            &accessibilityActionNames, "@@:");

        baseAccessibilityActionDescription = subclassMethod(macElementSubclass,
            macElementBaseClass, @selector(accessibilityActionDescription:),
            &accessibilityActionDescription, "@@:@");

        baseAccessibilityPerformAction = subclassMethod(macElementSubclass,
            macElementBaseClass, @selector(accessibilityPerformAction:),
            &accessibilityPerformAction, "v@:@");

        objc_registerClassPair(macElementSubclass);
    }
}

void macInitPanelSubclass()
{
    if(!macPanelBaseClass)
    {
        macPanelBaseClass = objc_getClass("QNSPanel");

        panelBaseAccessibilityAttributeValue = getBaseMethod<AccessibilityAttributeValueFunc>(macPanelBaseClass,
            @selector(accessibilityAttributeValue:));

        // Unlike the accessibility elements, we don't have any good point where
        // we can subclass the QNSPanel.  We just need to add one method to
        // enforce children navigation order though, so add it directly to the
        // QNSPanel class.
        subclassMethod(macPanelBaseClass, macPanelBaseClass,
            @selector(accessibilityChildrenInNavigationOrder),
            &panelAccessibilityChildrenInNavigationOrder, "@@:");
    }
}

QMacAccessibilityElement *macElementForId(QAccessible::Id accId)
{
    // macInitElementSubclass() is called by NativeAcc::init().
    Q_ASSERT(macElementBaseClass);
    Q_ASSERT(classElementWithIdFunc);

    return classElementWithIdFunc(macElementBaseClass,
                                  @selector(elementWithId:), accId);
}

QMacAccessibilityElement *macGetAccElement(QAccessibleInterface *pElement)
{
    if(!pElement)
        return nil;

    QAccessible::Id accId = QAccessible::uniqueId(pElement);
    return macElementForId(accId);
}

void macPostAccNotification(QAccessibleInterface &element, NSAccessibilityNotificationName event)
{
    // We don't emit any notifications if QAccessible isn't active yet.
    //
    // This is very subtle, but emitting a notification before QAccessible is
    // active can prevent us from overriding predefined QML Accessible
    // annotations on stock controls.  Basically, emitting the event actually
    // activates QAccessible, which causes stock controls to generate their own
    // annotations before our attached properties have been evaluated.
    //
    // If we did emit this event before QAccessible is active, what happens is:
    // - Cocoa sees the event and queries [QNSView accessibilityAttributeValue].
    // - QNSView turns on QAccessible since it was queried.
    // - QAccessible emits a notification that the activation has changed.
    // - Stock controls detect the change and query for their interfaces.
    //   (This is a bit dumb anyway, since they just do it to reflect table
    //   events, which is itself pretty dumb.)
    //
    // Since this would happen as soon as the first NativeAcc attached type is
    // evaluated, the rest are not set up yet and would not be overridden by
    // NativeAcc.
    //
    // There's no need to emit these events anyway if QAccessible isn't active,
    // since the screen reader necessarily has not observed anything from the
    // view yet.
    if(!QAccessible::isActive())
        return;

    QMacAccessibilityElement *pMacElement = macGetAccElement(&element);
    // These should be found, but it's not a big deal here if they're not - this
    // means that the object hasn't been observed yet, so any creation/deletion
    // messages shouldn't matter.
    if(pMacElement)
        NSAccessibilityPostNotification(pMacElement, event);
}

// Apply the subclass to an Objective-C object
void macSubclass(NSObject *object, Class expectedBase, Class subclass)
{
    if(!object)
    {
        qWarning() << "Cannot decorate invalid object";
        return;
    }

    Class actualElementClass = [object class];

    // If it's already subclassed, there's nothing to do.  This applies if the
    // object generates more than one 'created' event (it's destroyed and
    // recreated).
    if(actualElementClass == subclass)
        return;

    if(actualElementClass != expectedBase)
    {
        qWarning() << "Cannot subclass object of type:"
            << class_getName(actualElementClass);
        return;
    }

    object_setClass(object, subclass);
}

// Apply the subclass to a QMacAccessibilityElement.
void macSubclassElement(QMacAccessibilityElement *element)
{
    // macInitElementSubclass() is called by NativeAcc::init().
    Q_ASSERT(macElementBaseClass);
    Q_ASSERT(macElementSubclass);

    // We don't actually have the definition of QMacAccessibilityElement, so
    // reinterpret_cast to NSObject *.  This is valid because we know it's an
    // NSObject, and Objective-C does not support multiple inheritance.
    NSObject *elementObj = reinterpret_cast<NSObject *>(element);

    macSubclass(elementObj, macElementBaseClass, macElementSubclass);
}
