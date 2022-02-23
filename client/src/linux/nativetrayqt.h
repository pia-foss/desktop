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

#include "common.h"
#line HEADER_FILE("nativetrayqt.h")

#ifndef NATIVETRAYQT_H
#define NATIVETRAYQT_H

#include "nativetray.h"

#include <QHash>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>

// TrayIconShim is a shim around QSystemTrayIcon that allows it to be recreated.
//
// Qt has a dumb check when the QSystemTrayIcon is created to see if a
// StatusNotifierHost is registered, and it silently fails to create the icon if
// it isn't.  Maybe this was intended to fall back to XEmbed, but most desktops
// don't support that now anyway.
//
// The issue is when we launch on startup - we usually initialize before the
// StatusNotifierHost does, and Qt doesn't have any provision for a host
// appearing later; it failed at startup and it's hosed.  As a workaround, we
// wait 5s after startup when launched on login to create the icon, which is
// usually long enough for the StatusNotifierHost to load.
//
// Eventually, this will hopefully be replaced with a native D-Bus
// implementation.
class TrayIconShim : public QObject
{
    Q_OBJECT

public:
    // TrayIconShim does not initially create a system tray icon; call create()
    // to create it.  (Icon and tool tip changes are still stored before the
    // icon is created.)
    TrayIconShim(const QString &iconPath);

public:
    // Change the current icon - pass a path to an icon resource
    void setIcon(const QString &iconPath);
    // Show a message - see QSystemTrayIcon::showMessage().  message cannot be
    // empty.
    void showMessage(const QString &title, const QString &message,
                     const QString &iconPath);
    void setToolTip(const QString &toolTip);

    // (Re)create the underlying QSystemTrayIcon.  Pass the QMenu again; other
    // properties are stored by TrayIconShim and reapplied.
    void create(QMenu &menu);

signals:
    void activated(QSystemTrayIcon::ActivationReason reason);

private:
    QScopedPointer<QSystemTrayIcon> _pTrayIcon;
    // Store these values that we set in the tray icon so we know them when
    // recreating the icon
    // The menu isn't stored; NativeTrayQt keeps the QMenu around anyway so it
    // just passes the menu to recreate()
    QString _lastIconPath, _lastToolTip;
    // If a message is shown before the icon is created, it's queued here to be
    // shown in create().  Only one message can be queued; subsequent messages
    // overwrite the queue.
    // An empty _queuedMsg indicates that there is no queued message.
    QString _queuedMsgTitle, _queuedMsg, _queuedMsgIcon;
};

// NativeTrayQt is an implementation of NativeTray using Qt's QSystemTrayIcon.
// This is for use as a fallback on platforms where we haven't implemented a
// native tray icon.
class NativeTrayQt : public NativeTray
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("NativeTrayQt")

public:
    NativeTrayQt(IconState initialIcon, const QString &iconSet);

private:
    void onDestroyTimeout();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void setMenuItems(QMenu *menu, const NativeMenuItem::List &items);

    // Guess a geometry for the tray icon based on desktop environment.
    // The StatusNotifierItem interface doesn't provide any geoemtry info, but
    // we can make a somewhat reasonable guess since the exact position doesn't
    // have much effect (this just positions the popup-mode dashboard).
    //
    // See implementation for more details.
    QRect guessGeometry() const;

public:
    virtual void setIconState(IconState icon, const QString &iconSet) override;
    virtual void showNotification(IconState icon, const QString &title,
                                  const QString &subtitle) override;
    virtual void hideNotification() override;
    virtual void setToolTip(const QString &toolTip) override;
    virtual void getIconBound(QRect &iconBound, qreal &screenScale) override;
    virtual void setMenuItems(const NativeMenuItem::List &items) override;

public slots:
    void onMenuTriggered(QAction *action);

private:
    QMenu _menu;
    QString _lastIconSet;
    QHash<QString, QSharedPointer<QMenu>> _submenus;
    QHash<QString, QIcon> _menuIcons;
    // When running on startup, this timer is used to create the icon after
    // 5s to work around a Qt bug.
    QTimer _createTimer;
    TrayIconShim _trayIcon;
};

#endif
