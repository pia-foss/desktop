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
#line SOURCE_FILE("trayiconmanager.cpp")

#include "trayiconmanager.h"
#include "version.h"
#include "client.h"
#include "clientsettings.h"
#include <QDebug>
#include <QRect>
#include <QScreen>
#include <QGuiApplication>

TrayMetrics::TrayMetrics(const QRect &trayIconBound, const QRect &screenBound,
                         const QRect &workAreaBound, qreal screenScale)
    : _trayIconBound{trayIconBound}, _screenBound{screenBound},
      _workAreaBound{workAreaBound}, _screenScale{screenScale}
{
}

TrayIconManager::TrayIconManager(QObject *parent)
    : NativeTrayIconState{parent}, _icon{IconState::Disconnected}
{
    auto settings = Client::instance()->getInterface()->get_settings();
    _iconSet = settings->iconSet();
    _pTrayIcon = NativeTray::create(_icon, _iconSet);
    connect(_pTrayIcon.get(), &NativeTray::leftClicked, this,
            &TrayIconManager::onLeftClicked);
    connect(_pTrayIcon.get(), &NativeTray::menuItemSelected, this,
            &TrayIconManager::itemActivated);
    connect(settings, &ClientSettings::iconSetChanged, this, &TrayIconManager::onIconSetChanged);

    setToolTip(QStringLiteral(PIA_PRODUCT_NAME));
}

void TrayIconManager::setIcon(IconState iconState)
{
    if(iconState != _icon)
    {
        Q_ASSERT(_pTrayIcon);   // Class invariant
        _icon = iconState;
        _pTrayIcon->setIconState(iconState, _iconSet);
        emit iconChanged();
    }
}

void TrayIconManager::setToolTip(const QString& toolTip)
{
    if (toolTip != _toolTip)
    {
        _toolTip = toolTip;
        _pTrayIcon->setToolTip(toolTip);
        emit toolTipChanged();
    }
}

void TrayIconManager::showMessage(IconState notificationIcon,
                                  const QString &title,
                                  const QString &subtitle)
{
    _pTrayIcon->showNotification(notificationIcon, title, subtitle);
}

void TrayIconManager::hideMessage()
{
  _pTrayIcon->hideNotification();
}

void TrayIconManager::setMenuItems(const QJsonArray& items)
{
    if (items != _menuItems)
    {
        _menuItems = items;
        NativeMenuItem::List list;
        if (json_cast(items, list))
            _pTrayIcon->setMenuItems(list);
    }
}

TrayMetrics *TrayIconManager::getIconMetrics() const
{
    QRect iconBound{};
    qreal screenScale{1.0};

    _pTrayIcon->getIconBound(iconBound, screenScale);

    auto pMetrics = buildIconMetrics(iconBound, screenScale);
    return pMetrics.release();
}

auto TrayIconManager::buildIconMetrics(const QRect &iconBound,
                                       qreal screenScale) const
    -> std::unique_ptr<TrayMetrics>
{
    // Qt owns the QScreen objects referred to by these pointers; do not clean
    // them up.
    QList<QScreen *> screens{QGuiApplication::screens()};

    auto itIconScreen = std::find_if(screens.begin(), screens.end(),
        [&iconBound](auto pScreen)
        {
            return pScreen && pScreen->geometry().contains(iconBound);
        });
    // Default to the primary screen if no match was found.
    QScreen *pIconScreen = (itIconScreen != screens.end()) ? *itIconScreen : nullptr;
    if(!pIconScreen)
    {
        qWarning() << "Can't find screen containing icon bound"
            << iconBound << "- using primary screen";
        pIconScreen = QGuiApplication::primaryScreen();
    }

    // If there are no screens at all, use default values
    QRect screenBound{}, workAreaBound{};
    if(pIconScreen)
    {
        screenBound = pIconScreen->geometry();
        workAreaBound = pIconScreen->availableGeometry();
    }

    qDebug() << "Tray icon:" << iconBound << "- Screen bound:"
        << screenBound << "- work area:" << workAreaBound << "- scale:"
        << screenScale;

    return std::make_unique<TrayMetrics>(iconBound, screenBound, workAreaBound,
                                         screenScale);
}

void TrayIconManager::onLeftClicked(const QRect &clickedIconBound,
                                    qreal screenScale)
{
    auto pMetrics = buildIconMetrics(clickedIconBound, screenScale);
    emit trayClicked(pMetrics.get());
}

void TrayIconManager::onIconSetChanged()
{
    auto settings = Client::instance()->getInterface()->get_settings();
    if(settings->iconSet() != _iconSet)
    {
        Q_ASSERT(_pTrayIcon);   // Class invariant
        _iconSet = settings->iconSet();
        _pTrayIcon->setIconState(_icon, _iconSet);
    }
}
