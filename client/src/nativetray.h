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
#line HEADER_FILE("nativetray.h")

#ifndef NATIVETRAY_H
#define NATIVETRAY_H

#include "json.h"

#include <memory>

// NativeTray's IconState enumeration is exposed to QML by TrayIconManager.
// The Qt moc isn't smart enough to pick up an enum from a type alias though,
// so TrayIconManager can't just `using IconState = NativeTray::IconState`.
// Enums exposed to QML also have to be inside a QObject-derived type, so we
// can't just put this in global scope.
//
// As a result, we have to put this enum in a QObject-derived type that does
// nothing but define the enum.  (It's still better than spelling it out both
// places and static_cast<>ing throughout TrayIconManager.)
class NativeTrayIconState : public QObject
{
    Q_OBJECT

public:
    enum class IconState
    {
        Alert,
        Connected,
        Connecting,
        Disconnected,
        Disconnecting,
        Snoozed
    };
    Q_ENUM(IconState)
    using QObject::QObject;
};

// Class used to conveniently parse supplied menu definitions from QML/JavaScript.
class NativeMenuItem : public NativeJsonObject
{
    Q_OBJECT
public:
    NativeMenuItem() {}

    typedef QVector<QSharedPointer<NativeMenuItem>> List;

    // The text that appears in the menu
    JsonField(QString, text, {})
    // The code identifying this item in callbacks (required for all items except separators)
    JsonField(QString, code, {})
    // Determine whether item is clickable or grayed out
    JsonField(bool, enabled, true)
    // Determine whether item is checked (false = unchecked, null/undefined = uncheckable)
    JsonField(Optional<bool>, checked, nullptr)
    // The path to an image resource to draw in the menu
    JsonField(QString, icon, {})
    // A list of children, for creating a submenu
    JsonField(List, children, {})

    // Set this field alone to 'true' to declare a separator line
    JsonField(bool, separator, false)
};

// NativeTray is a relatively low-level abstraction over native tray and
// notification APIs.
//
// NativeTray (and its implementations) try not to define much application
// specific logic (such as notification text), these are mostly defined by
// TrayIconManager and/or the QML TrayManager.
//
// However, they do have to define some of it:
// - Specific icon states - implementations may use platform-specific theme
//   info to choose exactly which icon to show for a state.
// - Context menu items - allowing an arbitrary menu would require
//   implementing an abstraction layer like QMenu, since QMenu itself does not
//   provide access to the native menu handle needed to actually show the menu.
//   (And, some of the menu items change in platform-specific ways anyway, like
//   "Quit"/"Exit" and "Show Dashboard"/"Show dashboard".)
//
// NativeTray also does not hook up any properties/methods/etc. to QML,
// TrayIconManager does that.
//
// This abstraction exists due to a number of limitations and issues in Qt's
// QSystemTrayIcon, such as a lack of notification customization on OS X,
// inability to differentiate left/right clicks, and possible crashes on both
// OS X and Windows.
class NativeTray : public NativeTrayIconState
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("NativeTray")

public:
    enum class MenuItem
    {
        Exit,
    };
    Q_ENUM(MenuItem)

    // Create a NativeTray implementation appropriate for the current platform.
    // Provide the initial icon state to be displayed.
    //
    // QScopedPointer doesn't support move semantics, and its constructor from a
    // raw pointer is explicit, which makes it really cumbersome to use here -
    // use a std::unique_ptr instead.
    static std::unique_ptr<NativeTray> create(IconState initialIcon, const QString &initialIconSet);

public:
    // Set the state currently displayed by the tray icon.
    virtual void setIconState(IconState icon, const QString &iconSet) = 0;

    // Show a notification.  Replaces any notification already being shown.
    virtual void showNotification(IconState icon, const QString &title,
                                  const QString &subtitle) = 0;

    // Hide any notification that's currently shown (no-op if nothing is shown).
    virtual void hideNotification() = 0;

    // Specify all menu items as JSON array. Each item is a JSON object defined
    // by the NativeMenuItem class.
    virtual void setMenuItems(const NativeMenuItem::List& items) = 0;

    // Get the bound and screen scale of a tray icon.  There may be more than
    // one; there's no guarantee which this will return.  This is used to show
    // the dashboard initially at startup; any icon is OK since the user has not
    // directly interacted with it.
    virtual void getIconBound(QRect &iconBound, qreal &screenScale) = 0;

    // Set the tray tooltip
    virtual void setToolTip(const QString &toolTip) = 0;

signals:
    // The tray icon was clicked with the left mouse button.  (A full
    // press+release; normally shows the dashboard.)
    //
    // The bounds of the tray icon that was clicked are included (in screen
    // coordinates).  The scale factor that should be used when displaying the
    // dashboard on this screen is also included.
    void leftClicked(const QRect &clickedIconBound, qreal screenScale);

    // The menu is shown when the right mouse button is pressed.  (As mentioned
    // above, the menu contents are not configurable for lack of a menu
    // abstraction layer that provides access to the native menu handle.)
    //
    // When any menu item is selected, this signal is emitted with a MenuItem
    // value indicating which item was selected.  The bounds of the tray icon
    // that was clicked are also included (in screen coordinates).
    void menuItemSelected(QJsonValue code);
};

#endif
