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
#line HEADER_FILE("trayiconmanager.h")

#ifndef TRAYICONMANAGER_H
#define TRAYICONMANAGER_H

#include <QObject>
#include <QRect>
#include "nativetray.h"

// TrayMetrics contains the information about the tray icon that the dashboard
// needs to display itself.
//
// A TrayMetrics is passed with TrayIconManager::trayClicked(), and it can be
// queried with TrayIconManager::getIconMetrics().
//
// Platforms don't in general know when these metrics change, so the dashboard
// needs to query them when they're needed.  Generally, any user interaction
// (such as a click) should pass along a TrayMetrics for the dashboard to use
// to ensure that it uses the geometry of the correct icon (there may be more
// than one on OS X or Linux).  getIconMetrics() should normally only be used
// for non-quiet startup since there is no guarantee that it will use any
// particular icon's geometry.
class TrayMetrics : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(QRect trayIconBound READ getTrayIconBound FINAL CONSTANT)
    Q_PROPERTY(QRect screenBound READ getScreenBound FINAL CONSTANT)
    Q_PROPERTY(QRect workAreaBound READ getWorkAreaBound FINAL CONSTANT)
    Q_PROPERTY(qreal screenScale READ getScreenScale FINAL CONSTANT)

public:
    TrayMetrics(const QRect &trayIconBound, const QRect &screenBound,
                const QRect &workAreaBound, qreal screenScale);

public:
    const QRect &getTrayIconBound() const {return _trayIconBound;}
    const QRect &getScreenBound() const {return _screenBound;}
    const QRect &getWorkAreaBound() const {return _workAreaBound;}
    qreal getScreenScale() const {return _screenScale;}

private:
    QRect _trayIconBound, _screenBound, _workAreaBound;
    qreal _screenScale;
};

class TrayIconManager : public NativeTrayIconState
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("trayiconmanager")

public:
    Q_PROPERTY(IconState icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QString toolTip READ toolTip WRITE setToolTip NOTIFY toolTipChanged)

private:
    IconState _icon;
    QString _toolTip;
    std::unique_ptr<NativeTray> _pTrayIcon;
    QJsonArray _menuItems;
    QString _iconSet;

public:
    explicit TrayIconManager(QObject *parent = nullptr);

    IconState icon() const {return _icon;}
    void setIcon(IconState iconState);

    QString toolTip() const {return _toolTip;}
    void setToolTip(const QString &toolTip);

    // Show a popup message from the tray icon.
    Q_INVOKABLE void showMessage(IconState notificationIcon,
                                 const QString &title, const QString &subtitle);
    // Hide any popup message if one is being shown
    Q_INVOKABLE void hideMessage();

    // Specify context menu items as an array.
    Q_INVOKABLE void setMenuItems(const QJsonArray& items);
    // The QML engine owns the returned TrayMetrics object.
    Q_INVOKABLE TrayMetrics *getIconMetrics() const;

private:
    std::unique_ptr<TrayMetrics> buildIconMetrics(const QRect &iconBound,
                                                  qreal screenScale) const;

signals:
    // The tray icon was clicked.  Includes a TrayMetrics containing the icon
    // and screen bounds, work area, etc.
    // The QML engine requires us to emit this signal with a pointer to the
    // TrayMetrics to bridge to QML; TrayIconManager owns the TrayMetrics
    // object.
    void trayClicked(TrayMetrics *pMetrics);
    void quitClicked();
    void iconChanged();
    void toolTipChanged();
    void itemActivated(const QJsonValue &code);

private slots:
    void onLeftClicked(const QRect &clickedIconBound, qreal screenScale);
    void onIconSetChanged ();
};

#endif // TRAYICONMANAGER_H
