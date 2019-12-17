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
#line HEADER_FILE("win_appmanifest.h")

#ifndef WIN_APPMANIFEST_H
#define WIN_APPMANIFEST_H

#include <unordered_set>

struct AppExecutables
{
    std::unordered_set<QString> executables;
    bool usesWwa;
};

// Read the manifest for a UWP app to determine what executables it launches,
// and whether it uses WWA.
bool inspectUwpAppManifest(const QString &installDir, AppExecutables &appExes);

#endif
