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
#line HEADER_FILE("win/win_tray.h")

#ifndef WIN_TRAY_H
#define WIN_TRAY_H

#include "../nativetray.h"
#include "win_objects.h"
#include "win_scaler.h"
#include <common/src/win/win_messagewnd.h>
#include <common/src/win/win_util.h>
#include <brand.h>

#include <QAbstractNativeEventFilter>
#include <QTimer>
#include <QScopedPointer>
#include <QHash>
#include <QWinEventNotifier>

std::unique_ptr<NativeTray> createNativeTrayWin(NativeTray::IconState initialIcon, const QString &initialIconSet);

class ShellTrayIcon;

class TrayCreatedReceiver : public MessageWnd
{
public:
    TrayCreatedReceiver(ShellTrayIcon *tray);
    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    UINT _trayCreatedCode;
    ShellTrayIcon *_tray;
};

// The objects below should not be referenced anywhere but win_tray.cpp, but
// they are in the header file in order for the Qt moc to pick them up correctly
// in both normal and combined-source builds.

// Owns and manages a Win32 shell tray icon.
class ShellTrayIcon : public QObject, public MessageWnd
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("shelltrayicon")

private:
    enum : unsigned
    {
        // Message ID used by the tray icon
        IconMsg = WM_USER
    };

    // The icon is specified using a GUID, the preferred method since Vista.
    // The major benefit of this is that we won't get extra icons hanging around
    // if the app crashes - a new instance of the app can delete the old icon.
    static QUuid _iconId;

    // The problem with the GUID icon identifier is that it becomes tied to the
    // executable path for unsigned executables - an executable at a different
    // location won't be allowed to create the icon (including switching debug/
    // release, client/client-portable, etc.).  This is a nuisance for
    // development.  As a middle ground, the UUID is set based on the path to
    // the application, so copies of the application in different locations will
    // use different IDs.
    static QUuid _iconGuidNamespace;
    static void initIconGuid();

    // Reused tray icon descriptor struct (preinitialized with flags etc.)
    static NOTIFYICONDATAW _defaultIconData;

    // Store the last used icon when re-creating the icon to recover from things like
    // explorer.exe crash
    HICON _lastUsedIcon;

    TrayCreatedReceiver _explorerCrashReceiver;

public:
    ShellTrayIcon(HICON initialIcon);
    ~ShellTrayIcon();
    void redrawIcon();

private:
    ShellTrayIcon(const ShellTrayIcon &) = delete;
    ShellTrayIcon &operator=(const ShellTrayIcon &) = delete;

signals:
    void leftClicked();
    void showMenu(const QPoint &menuPos);

private:
    bool deleteIcon();
    LRESULT handleIconMsg(WORD msg, int x, int y);
    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    void createIcon(HICON icon);

public:
    QRect getIconBound() const;
    void setIcon(HICON trayIcon);
    void showNotification(HICON notificationIcon, const QString &title,
                          const QString &message);
    void hideNotification();
    void setToolTip(const QString &toolTip);

private:
    bool _recentKeySelect;


};

class IconTheme
{
private:
    IconResource _alert;
    IconResource _connected;
    IconResource _connecting;
    IconResource _disconnecting;
    IconResource _disconnected;
    IconResource _snoozed;
public:
    IconTheme(IconResource::Size size, WORD alert_code, WORD connected_code, WORD connecting_code, WORD disconnecting_code, WORD disconnected_code, WORD snoozed_code);
    const IconResource &getIconForState (NativeTray::IconState state) const;
};

class TrayIconLoader
{
private:
    // Choose either the no-outline icon or outlined icon based on the Windows
    // version
    static WORD iconForWinVer(WORD noOutlineId, WORD outlineId);
public:
    // Whether to use outlined tray icons (used by NativeHelpers)
    static bool useOutlineIcons();

public:
    TrayIconLoader(IconResource::Size size, const QString &initialIconSet);
    const IconResource &getStateIcon(NativeTray::IconState state) const;
    void setIconSet(const QString &iconSet);
    void setOSThemeSetting(const QString &theme);

private:
    QString _iconSet;
    QString _osThemeSetting;
    IconResource::Size _size;

#if BRAND_HAS_CLASSIC_TRAY
    IconTheme _theme_classic;
#endif
    // 'Light' and 'Colored' use either outlined or non-outlined icons based on
    // the Windows version
    IconTheme _theme_light;
    IconTheme _theme_dark;
    IconTheme _theme_colored;
};

class NativeTrayWin : public NativeTray
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("nativetraywin")

private:
    enum MenuItemId : int
    {
        NoSelection = 0, // WinPopupMenu::showMenu() returns 0 for no selection
        Exit,
    };

public:
    NativeTrayWin(IconState initialIcon, const QString &iconSet);

public:
    virtual void setIconState(IconState icon, const QString &iconSet) override;
    virtual void showNotification(IconState icon, const QString &title,
                                  const QString &subtitle) override;
    virtual void hideNotification() override;
    virtual void setToolTip(const QString &toolTip) override;
    virtual void setMenuItems(const NativeMenuItem::List& items) override;
    virtual void getIconBound(QRect &iconBound, qreal &screenScale) override;

    void updateIconWithSysTheme();
private:
    void onLeftClicked();
    void onShowMenu(const QPoint &pos);
    const IconResource &getStateIcon(IconState icon, const QString &iconSet);
    void onThemeRegChanged(HANDLE hEvent);
    void startWatchThemeReg();
    void activateIconSet(const QString &iconSet);

private:
    TrayIconLoader _trayIcons;
    TrayIconLoader _notificationIcons;
    ShellTrayIcon _icon;
    WinPopupMenu _menu;
    MonitorScale _monitorScale;
    QWinEventNotifier _eventNotifier;
    IconState _iconState;
    WinHandle _themeChangeEvent;
    WinHKey _themeKey;
    DWORD _themeValue;
};

#endif
