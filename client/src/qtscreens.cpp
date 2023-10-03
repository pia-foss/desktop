// Copyright (c) 2023 Private Internet Access, Inc.
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
#line SOURCE_FILE("qtscreens.cpp")

#include "qtscreens.h"
#include <QGuiApplication>
#include <QScreen>

// Implementation of PlatformScreens using QScreen - used on Windows and Linux.
class QtScreens : public PlatformScreens
{
    Q_OBJECT

public:
    QtScreens();

private:
    void connectScreen(QScreen &screen);
    std::vector<Screen> buildScreens();
    void rebuildScreens();
};

QtScreens::QtScreens()
{
    Q_ASSERT(qGuiApp);  // Can't create these before the app is created

    // Connect to all the screens already present, and to the screenAdded/
    // screenRemoved signals
    connect(qGuiApp, &QGuiApplication::screenAdded, this,
            [this](QScreen *pNewScreen)
            {
                if(!pNewScreen)
                    return;
                connectScreen(*pNewScreen);
                rebuildScreens();
            });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this,
            &QtScreens::rebuildScreens);
    // We don't need to disconnect anything from a screen that's being removed,
    // it's being destroyed anyway.
    connect(qGuiApp, &QGuiApplication::screenRemoved, this,
            &QtScreens::rebuildScreens);
    for(auto *pScreen : qGuiApp->screens())
    {
        if(pScreen)
            connectScreen(*pScreen);
    }

    rebuildScreens();
}

void QtScreens::connectScreen(QScreen &screen)
{
    // Detect when the available geometry changes.
    // These signals need to cover the properties used by buildScreens().
    connect(&screen, &QScreen::availableGeometryChanged, this,
            &QtScreens::rebuildScreens);
}

auto QtScreens::buildScreens() -> std::vector<Screen>
{
    std::vector<Screen> newScreens;
    const QScreen *pPrimary = qGuiApp->primaryScreen();
    QList<QScreen *> allScreens = qGuiApp->screens();

    newScreens.reserve(allScreens.size());
    for(const auto &pQtScreen : allScreens)
    {
        if(!pQtScreen)
            continue;

        newScreens.push_back({pQtScreen == pPrimary, pQtScreen->geometry(),
                              pQtScreen->availableGeometry()});
    }

    return newScreens;
}

void QtScreens::rebuildScreens()
{
    updateScreens(buildScreens());
}

std::unique_ptr<PlatformScreens> createQtScreens()
{
    return std::unique_ptr<PlatformScreens>{new QtScreens{}};
}

#include "qtscreens.moc"
