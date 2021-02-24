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
#line SOURCE_FILE("platformscreens.cpp")

#include <memory>

#if defined(Q_OS_MAC)
    #include "mac/mac_screens.h"
#else
    #include "qtscreens.h"
#endif

namespace
{
    std::unique_ptr<PlatformScreens> pImpl;
}

PlatformScreens &PlatformScreens::instance()
{
    if(!pImpl)
    {
#if defined(Q_OS_MAC)
        pImpl = createMacScreens();
#else
        pImpl = createQtScreens();
#endif
        Q_ASSERT(pImpl);    // Postcondition of create{Mac,Qt}Screens()
    }

    return *pImpl;
}

void PlatformScreens::updateScreens(std::vector<Screen> newScreens)
{
    qInfo() << "Updating screens due to notification";
    if(_screens != newScreens)
    {
        const auto &traceScreens = [](const std::vector<Screen> &screens)
        {
            for(int i=0; i<screens.size(); ++i)
            {
                const auto &screen = screens[i];
                qInfo() << "-" << i << screen.geometry() << screen.availableGeometry()
                    << (screen.primary() ? "(primary)" : "");
            }
        };
        qInfo() << "Screens changed from (" << _screens.size() << "):";
        traceScreens(_screens);
        qInfo() << "to (" << newScreens.size() << "):";
        traceScreens(newScreens);

        _screens = std::move(newScreens);
        emit screensChanged();
    }
    else
    {
        qInfo() << "Screens did not change (have" << _screens.size() << "screens)";
    }
}

auto PlatformScreens::getPrimaryScreen() const -> const Screen *
{
    for(const auto &screen : getScreens())
    {
        if(screen.primary())
            return &screen;
    }
    return nullptr;
}
