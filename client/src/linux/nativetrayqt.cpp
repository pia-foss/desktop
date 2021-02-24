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
#line SOURCE_FILE("nativetrayqt.cpp")

#include "nativetrayqt.h"
#include "brand.h"
#include "version.h"
#include "client.h"
#include "platformscreens.h"
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QPainter>
#include <chrono>

#ifndef Q_OS_LINUX
#error "This implementation is only to be used on Linux"
#endif

#include "linux_env.h"
#include "linux_scaler.h"
#include "linux_language.h"
#include "brand.h"

namespace
{
    QString getIconForState(NativeTray::IconState icon, const QString &iconSet)
    {
        QString baseName;
        if(iconSet == QStringLiteral("dark"))
            baseName = QStringLiteral("dark-no-outline-margins");
        else if(iconSet == QStringLiteral("colored"))
            baseName = QStringLiteral("colored-no-outline-margins");
        else if(BRAND_HAS_CLASSIC_TRAY && iconSet == QStringLiteral("classic"))
            baseName = QStringLiteral("classic-margins");
        // light, or fallback for invalid theme name
        else
            baseName = QStringLiteral("light-no-outline-margins");

        QString stateName;
        switch(icon)
        {
        case NativeTray::IconState::Alert:
            stateName = QStringLiteral("alert");
            break;
        case NativeTray::IconState::Disconnected:
            stateName = QStringLiteral("down");
            break;
        case NativeTray::IconState::Connected:
            stateName = QStringLiteral("connected");
            break;
        case NativeTray::IconState::Disconnecting:
            stateName = QStringLiteral("disconnecting");
            break;
        case NativeTray::IconState::Connecting:
            stateName = QStringLiteral("connecting");
            break;
        case NativeTray::IconState::Snoozed:
            stateName = QStringLiteral("snoozed");
            break;
        default:
            // Impossible, handled all states (ensures valid stateName below)
            Q_ASSERT(false);
            break;
        }
        QString resource = QStringLiteral(":/img/tray/square-%1-%2.png").arg(baseName).arg(stateName);
        // Should always end up with a valid file name here - consequence of
        // the checks above on the theme and state, and all these resources
        // must exist
        Q_ASSERT(QFile::exists(resource));
        return resource;
    }
}

TrayIconShim::TrayIconShim(const QString &iconPath)
    : _lastIconPath{iconPath}
{
}

void TrayIconShim::setIcon(const QString &iconPath)
{
    _lastIconPath = iconPath;
    if(_pTrayIcon)
        _pTrayIcon->setIcon(QIcon{iconPath});
}

void TrayIconShim::showMessage(const QString &title, const QString &message,
                               const QString &iconPath)
{
    Q_ASSERT(!message.isEmpty());   // Can't be empty, would indicate empty queue

    if(_pTrayIcon)
        _pTrayIcon->showMessage(title, message, QIcon{iconPath});
    else
    {
        // Queue the message and show it when the icon is created
        _queuedMsgTitle = title;
        _queuedMsg = message;
        _queuedMsgIcon = iconPath;
    }
}

void TrayIconShim::setToolTip(const QString &toolTip)
{
    _lastToolTip = toolTip;
    if(_pTrayIcon)
        _pTrayIcon->setToolTip(toolTip);
}

void TrayIconShim::create(QMenu &menu)
{
    // Destroy the old icon (if there is one) before creating the new one
    _pTrayIcon.reset();
    // Create the new icon and set invariant state
    _pTrayIcon.reset(new QSystemTrayIcon{QIcon{_lastIconPath}});
    _pTrayIcon->setVisible(true);
    _pTrayIcon->setContextMenu(&menu);
    QObject::connect(_pTrayIcon.data(), &QSystemTrayIcon::activated, this,
                     &TrayIconShim::activated);
    // Restore stored state
    _pTrayIcon->setIcon(QIcon{_lastIconPath});
    _pTrayIcon->setToolTip(_lastToolTip);

    // If there's a queued message, show it
    if(!_queuedMsg.isEmpty())
    {
        _pTrayIcon->showMessage(_queuedMsgTitle, _queuedMsg, QIcon{_queuedMsgIcon});
        _queuedMsgTitle.clear();
        _queuedMsg.clear();
        _queuedMsgIcon.clear();
    }
}

NativeTrayQt::NativeTrayQt(IconState initialIcon, const QString &iconSet)
    : _trayIcon{getIconForState(initialIcon, iconSet)}
{
    _lastIconSet = iconSet;
    connect(&_trayIcon, &TrayIconShim::activated, this,
            &NativeTrayQt::onTrayActivated);
    // Handle menu items clicked
    connect(&_menu, &QMenu::triggered, this, &NativeTrayQt::onMenuTriggered);

    // When launched on startup, recreate the icon after 5 seconds, to give Qt
    // one more shot to detect the tray correctly.
    //
    // There's no way to tell whether Qt actually creates the icon, so we can't
    // try to only do this if it failed, or try again if it still fails, etc.
    // This creates a noticeable blip in the tray icon, so we only want to do it
    // when launched on startup.
    //
    // Using the quiet flag to detect a launch on startup is a bit of a kludge,
    // but eventually we want to replace this with a direct D-Bus implementation
    // anyway, and the only side effect of this workaround is a brief blip in
    // the tray icon (hopefully).
    if(Client::instance()->getInterface()->get_state()->quietLaunch())
    {
        QObject::connect(&_createTimer, &QTimer::timeout, this,
                        [this](){_trayIcon.create(_menu);});
        _createTimer.setSingleShot(true);
        _createTimer.start(std::chrono::seconds{5});
    }
    else
    {
        _trayIcon.create(_menu);
    }
}

void NativeTrayQt::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::ActivationReason::Trigger)
    {
        // This event has to be handled differently based on the desktop
        // environment.
        //
        // Except for GNOME, all environments tested either expect us to show
        // the window on Trigger or never generate it at all.  GNOME is the
        // oddball due to disliking tray icons generally.  Out of paranoia
        // though, we're only handling the Trigger event on environments where
        // we've verified it makes sense, others will ignore it unless we add
        // them here.
        //
        // LXDE - Never generates this event, ignored.
        switch(LinuxEnv::getDesktop())
        {
        default:
        case LinuxEnv::Desktop::Unknown:
            // Ignore by default as the paranoid option.  All tested
            // environments other than GNOME are fine with showing on Trigger,
            // but it's hard to be sure what other untested environments would
            // do, and there are a lot of GNOME variants out there that we could
            // fail to detect.
            break;
        case LinuxEnv::Desktop::GNOME:
            // GNOME - Ignore this event.  Complicated due to GNOME disliking
            // tray icons generally, ignoring is least-bad alternative.
            //   - Tray addon in menu bar (default in Ubuntu) - Bizarrely
            //     generates Trigger after a double-click, which also shows and
            //     hides the menu.  Popping the app after showing and hiding the
            //     menu is kind of strange, most GNOME apps don't do this.
            //   - Lower-left hidden tray (default in Debian) - Behaves like
            //     KDE, left-click generates Trigger.  Popping here isn't that
            //     weird, but most GNOME apps don't do it.
            // We can't really figure out what the tray under GNOME is doing
            // with these events, so we always ignore Trigger under GNOME as the
            // least-bad compromise.
            break;
        // These desktops never generate Trigger; ignore it (just for paranoia)
        case LinuxEnv::Desktop::Unity:
        case LinuxEnv::Desktop::LXDE:
        case LinuxEnv::Desktop::Pantheon:   // No tray, and no readily available tray addons
            break;
        // Show on Activate for these distributions
        case LinuxEnv::Desktop::XFCE:
        case LinuxEnv::Desktop::KDE:
        case LinuxEnv::Desktop::LXQt:
        case LinuxEnv::Desktop::Deepin:
        case LinuxEnv::Desktop::Cinnamon:
        case LinuxEnv::Desktop::MATE:
            // KDE, Deepin, Cinnamon, MATE - Generates Trigger on tray left-click; should
            //   show the window.
            // XFCE - In Arch/Antergos, behaves like KDE.  In Xubuntu, default is
            //   like LXDE, but turning off the "Menu is default action" option
            //   behaves like KDE.  In all cases, correct behavior is to show on
            //   Activate.
            // Load the scale factor now if it hasn't been loaded yet, we're
            // about to compute metrics to show the dashboard
            LinuxWindowScaler::initScaleFactor();
            emit leftClicked(guessGeometry(), LinuxWindowScaler::getScaleFactor());
            break;
        }
    }
}

void NativeTrayQt::setMenuItems(QMenu *menu, const NativeMenuItem::List &items)
{
    menu->clear();
    for (const auto &item : items)
    {
        if (item->separator())
        {
            menu->addSeparator();
            continue;
        }
        QAction *action = menu->addAction(item->text());
        action->setEnabled(item->enabled());
        action->setCheckable(item->checked() != nullptr);
        action->setChecked(item->checked() == true);
        action->setData(item->code());
        if (!item->icon().isEmpty())
        {
            auto it = _menuIcons.find(item->icon());
            if (it != _menuIcons.end())
                action->setIcon(*it);
            else
            {
                QPixmap pixmap(item->icon());
                if (!pixmap.isNull())
                {
                    if (pixmap.width() != pixmap.height())
                    {
                        int max = pixmap.width() > pixmap.height() ? pixmap.width() : pixmap.height();
                        QPixmap scaled(max, max);
                        scaled.fill(Qt::transparent);
                        {
                            QPainter painter(&scaled);
                            painter.drawPixmap((max - pixmap.width()) / 2, (max - pixmap.height()) / 2, pixmap);
                        }
                        pixmap = scaled;
                    }
                    QIcon icon(pixmap);
                    _menuIcons.insert(item->icon(), icon);
                    action->setIcon(icon);
                }
            }
        }
        if (!item->children().isEmpty())
        {
            auto &submenu = _submenus[item->code()];
            if (!submenu)
            {
                submenu.reset(new QMenu());
                connect(submenu.get(), &QMenu::triggered, this, &NativeTrayQt::onMenuTriggered);
            }
            setMenuItems(submenu.get(), item->children());
            action->setMenu(submenu.get());
        }
    }
}

QRect NativeTrayQt::guessGeometry() const
{
    // As mentioned in the declaration, QSystemTrayIcon::geometry() does not
    // work on StatusNotifierItem-backed QSystemTrayIcons, because the
    // StatusNotifierItem interface doesn't provide any way to get geometry
    // from the StatusNotifierHost(s).
    //
    // It does provide click positions for Activate, which would be very
    // helpful, but Qt doesn't forward that to us.  We might later use
    // StatusNotifierItem over D-Bus directly to get that info.  We could get
    // the current cursor position with QCursor::pos(), but that wouldn't be
    // very accurate:
    // - activations with the keyboard, context menu, etc., would be wrong; the
    //   cursor isn't pointing to the tray icon
    // - if the computer is slowing down even a little, the user could move the
    //   cursor in between clicking the icon and the query for the position
    //
    // Since the geometry only influences the popup-mode dashboard placement, we
    // can make a pretty reasonable guess based on the desktop environment.  We
    // just choose a corner based on environment and then return a geometry that
    // will place the dashboard there.
    //
    // The windowed dashboard is still the default on Linux because this isn't
    // going to be perfect, but works a lot of the time.  It's probably pretty
    // accurate for single-display environments, which covers a lot of users.
    // Multiple displays will probably be less accurate, particularly because
    // QScreen::availableGeometry() does not work with multiple displays.

    // Assume the tray is on the primary screen (not necessarily correct, but a
    // reasonable guess).
    const PlatformScreens::Screen *pPrimaryScreen = PlatformScreens::instance().getPrimaryScreen();
    if(!pPrimaryScreen)
        return {};  // Can't guess, no screens

    QRect screenBound = pPrimaryScreen->geometry();
    QRect workArea = pPrimaryScreen->availableGeometry();
    QRect iconGuess{0, 0, 1, 1};

    // Most of the guesses below assume LTR - they assume the tray is at the
    // right end of a horizontal taskbar, etc.  If the desktop is actually RTL,
    // flip at the end.
    bool rtlFlip = LinuxEnv::isDesktopRtl();

    // If Qt can't give us the work area, we have to guess the taskbar size to
    // avoid overlapping it.
    const int taskbarSizeGuess = 60;

    // Check the work area for this screen.  If there's a margin on the top or
    // bottom, it's probably a taskbar, so assume the tray is there.  If both
    // the top and bottom have margins, prefer the top.
    //
    // Top/bottom margins are favored over right/left margins; some desktops
    // (Unity) have both top and left by default, etc.  The top/bottom is more
    // likely to be a panel.
    //
    // This is only checked for the primary screen.  We could check it for all
    // screens before falling back to a desktop environment guess, but wouldn't
    // generate a better guess, because QScreen::availableGeometry() doesn't
    // work when there are multiple displays.
    if(workArea.top() > screenBound.top())
    {
        // Top margin - move to top-right
        iconGuess.moveBottomRight(workArea.topRight());
    }
    else if(workArea.bottom() < screenBound.bottom())
    {
        // Bottom margin - move to bottom-right
        iconGuess.moveTopRight(workArea.bottomRight());
    }
    // There are no top/bottom margins.  If there's a left or right margin, the
    // panel is probably on that side.  Guess that the tray is probably at the
    // bottom.  Prefer the right side if both have margins.
    else if(workArea.right() < screenBound.right())
    {
        // Right margin - move to bottom-right
        iconGuess.moveBottomLeft(workArea.bottomRight());
        // Don't flip for RTL, we know the panel is actually on this side, we're
        // not assuming a layout direction within the panel
        rtlFlip = false;
    }
    else if(workArea.left() > screenBound.left())
    {
        // Left margin - move to bottom-left
        iconGuess.moveBottomRight(workArea.bottomLeft());
        // As above, don't flip for RTL
        rtlFlip = false;
    }
    // Qt can't always give us the screen work area due to X11 limitations
    // (usually with multiple displays), so in that case, guess based on the
    // default for the desktop environment.  Most of these environments allow
    // the user to move panels/widgets anywhere, so this is just a guess.
    //
    // For all these cases, we know there are no top/bottom margins on the work
    // area, so position inside the screen bound.
    else
    {
        switch(LinuxEnv::getDesktop())
        {
        // Top-right - default for GNOME, Unity, XFCE, Pantheon
        // Guess this for unknown desktops due to all the GNOME variants out
        // there.  (Certainly better than top-left, which is universally wrong
        // and would be the default if we did nothing.)
        default:
        case LinuxEnv::Desktop::Unknown:
        case LinuxEnv::Desktop::GNOME:
        case LinuxEnv::Desktop::Unity:
        case LinuxEnv::Desktop::XFCE:
        case LinuxEnv::Desktop::Pantheon:   // Doesn't have a tray, indicators are top-right
            workArea.setTop(screenBound.top() + taskbarSizeGuess);
            iconGuess.moveBottomRight(workArea.topRight());
            break;
        // Bottom-right - KDE, LXDE, LXQt, Deepin, Cinnamon, MATE
        // Deepin uses an OS X-style dock by default, so the tray isn't quite in
        // the corner, but this corner is the most reasonable.
        case LinuxEnv::Desktop::KDE:
        case LinuxEnv::Desktop::LXDE:
        case LinuxEnv::Desktop::LXQt:
        case LinuxEnv::Desktop::Deepin:
        case LinuxEnv::Desktop::Cinnamon:
        case LinuxEnv::Desktop::MATE:
            workArea.setBottom(screenBound.bottom() - taskbarSizeGuess);
            iconGuess.moveTopRight(workArea.bottomRight());
            break;
        }
    }

    if(rtlFlip)
    {
        qInfo() << "Desktop is RTL, flip icon geometry" << iconGuess;
        // Distance of guessed left X from screen's right edge (positive as long
        // as icon bound is actually in the desired screen)
        int leftFromEdge = screenBound.right() - iconGuess.left();
        iconGuess.moveRight(screenBound.left() + leftFromEdge);
    }

    // Make sure the icon bound is on the desired screen.
    if(iconGuess.right() > screenBound.right())
        iconGuess.moveRight(screenBound.right());
    if(iconGuess.left() < screenBound.left())
        iconGuess.moveLeft(screenBound.left());
    if(iconGuess.bottom() > screenBound.bottom())
        iconGuess.moveBottom(screenBound.bottom());
    if(iconGuess.top() < screenBound.top())
        iconGuess.moveTop(screenBound.top());

    qInfo() << "guess tray icon" << iconGuess << "with screen" << screenBound
        << "and work area" << workArea;
    return iconGuess;
}

void NativeTrayQt::setIconState(IconState icon, const QString &iconSet)
{
    _lastIconSet = iconSet;
    _trayIcon.setIcon(getIconForState(icon, iconSet));
}

void NativeTrayQt::showNotification(IconState icon, const QString &title,
                                    const QString &subtitle)
{
    // Use the product name if the subtitle is empty.
    const QString &message = subtitle.isEmpty() ? QStringLiteral(PIA_PRODUCT_NAME) : subtitle;
    _trayIcon.showMessage(title, message, getIconForState(icon, _lastIconSet));
}

void NativeTrayQt::hideNotification()
{
    // Can't implement this, not provided by QSystemTrayIcon.
}

void NativeTrayQt::setToolTip(const QString& toolTip)
{
    _trayIcon.setToolTip(toolTip);
}

void NativeTrayQt::getIconBound(QRect &iconBound, qreal &screenScale)
{
    iconBound = guessGeometry();
    // Load the scale factor now if it hasn't been loaded yet, we're
    // about to compute metrics to show the dashboard
    LinuxWindowScaler::initScaleFactor();
    screenScale = LinuxWindowScaler::getScaleFactor();
}

void NativeTrayQt::setMenuItems(const NativeMenuItem::List &items)
{
    setMenuItems(&_menu, items);
}

void NativeTrayQt::onMenuTriggered(QAction *action)
{
    QString code;
    if (action->data().userType() == QMetaType::QString)
    {
        code = action->data().toString();
    }
    qDebug () << "Menu was triggered" << code;
    emit menuItemSelected(code);
}
