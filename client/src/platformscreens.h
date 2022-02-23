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
#line HEADER_FILE("platformscreens.h")

#ifndef PLATFORMSCREENS_H
#define PLATFORMSCREENS_H

#include <QRect>

// Abstraction for QScreen functionality so it can be replaced on Mac - covers:
// * QGuiApplication: screens(), primaryScreen()
// * QScreen: availableSize() availableGeometry(), geometry()
// * change notifications
//
// Qt's QScreen change notifications are broken on Mac in Qt 5.15 - toggling
// Mirror Displays causes the application to crash due to accessing QScreen
// objects that have been removed.  (It also looks like QScreen has had issues
// on Mac in the past and regresses frequently.)
//
// Fortunately, if we don't touch QScreen at all, Qt doesn't hook up these
// change notifications, and we only use a few parts of QScreen.  Use native
// APIs on Mac instead.
class PlatformScreens : public QObject
{
    Q_OBJECT

public:
    // Description of a screen
    class Screen
    {
    public:
        Screen(bool primary, QRect geometry, QRect availableGeometry)
            : _primary{primary}, _geometry{geometry},
              _availableGeometry{availableGeometry}
        {
        }

        bool operator==(const Screen &other) const
        {
            return primary() == other.primary() &&
                geometry() == other.geometry() &&
                availableGeometry() == other.availableGeometry();
        }

    public:
        bool primary() const {return _primary;}
        const QRect &geometry() const {return _geometry;}
        QSize size() const {return _geometry.size();}
        const QRect &availableGeometry() const {return _availableGeometry;}
        QSize availableSize() const {return _availableGeometry.size();}

    private:
        // Whether this is the primary (true for at most one screen)
        bool _primary;
        // The screen's location and size in the virtual desktop.  On Windows
        // and Linux, this is the physical pixel size since PIA draws in
        // physical coordinates (we do our own scaling).  On Mac, this is the
        // logical pixel size, since the OS handles scaling.
        QRect _geometry;
        // Available geometry - the part of 'geometry' that can be used by
        // applications; excludes menus/taskbar/Dock/etc.
        QRect _availableGeometry;
    };

public:
    // Get the implementation of PlatformScreens for this platform.
    static PlatformScreens &instance();

public:
    virtual ~PlatformScreens() = default;

protected:
    // When the screens have changed, pass the new set of screens here - this
    // traces, updates _screens, and emits the screensChanged() signal (if the
    // screens actually changed)
    void updateScreens(std::vector<Screen> newScreens);

public:
    // Get the primary screen (returns the element from getScreens() with
    // primary == true).  If no primary screen was found, this returns
    // nullptr.
    //
    // The returned Screen is owned by PlatformScreens and remains valid until
    // the next change notification.
    const Screen *getPrimaryScreen() const;

    // Get all screens.
    const std::vector<Screen> &getScreens() const {return _screens;}

signals:
    // The screens have changed in some way.  There's no indication what exactly
    // has changed.  getScreens() and getPrimaryScreen() are up to date when
    // this is emitted.
    void screensChanged();

private:
    std::vector<Screen> _screens;
};

#endif
