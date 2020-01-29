// Copyright (c) 2020 Private Internet Access, Inc.
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
#line SOURCE_FILE("workspacechange.cpp")

#include "workspacechange.h"
#include <QGuiApplication>

WorkspaceChange::WorkspaceChange()
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
                emitWorkspaceChange();
            });
    // We don't need to disconnect anything from a screen that's being removed,
    // it's being destroyed anyway.
    connect(qGuiApp, &QGuiApplication::screenRemoved, this,
            &WorkspaceChange::emitWorkspaceChange);
    for(auto *pScreen : qGuiApp->screens())
    {
        if(pScreen)
            connectScreen(*pScreen);
    }
}

void WorkspaceChange::connectScreen(QScreen &screen)
{
    // Detect when the available geometry changes.
    // These signals need to cover the properties used by
    // WindowMaxSize::calcEffectiveSize().  WorkspaceChange is also used by
    // DashboardPopup to detect changes in the work area.
    connect(&screen, &QScreen::availableGeometryChanged, this,
            &WorkspaceChange::emitWorkspaceChange);
}

void WorkspaceChange::emitWorkspaceChange()
{
    qInfo() << "Workspace changed";
    emit workspaceChanged();
}
