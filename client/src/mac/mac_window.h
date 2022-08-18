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

#include <common/src/common.h>
#line HEADER_FILE("mac/mac_window.h")
#include <QPixmap>

#ifndef MAC_WINDOW_H
#define MAC_WINDOW_H

#include "../windowmaxsize.h"

// Put a window on all workspaces on OS X (enables
// NSWindowCollectionBehaviorCanJoinAllSpaces)
void macSetAllWorkspaces(QWindow &window);
void enableShowInDock();
void disableShowInDock();

// Create the NativeWindowMetrics implementation.
std::unique_ptr<NativeWindowMetrics> macCreateWindowMetrics();

// Check if no window is currently focused on Mac, and deactivate the app if so.
// This is necessary to focus the next application if the last PIA window is
// closed.
void macCheckAppDeactivate();

#endif //MAC_WINDOW_H
