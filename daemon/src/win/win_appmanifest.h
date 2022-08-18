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

#pragma once
#include <common/src/common.h>
#include <unordered_set>

struct AppExecutables
{
    std::unordered_set<std::wstring> executables;
    bool usesWwa;
};

// Read the manifest for a UWP app to determine what executables it launches,
// and whether it uses WWA.
// 
// This can be called more than once using the same AppExecutables; new
// executables are added to any existing contents.  Similarly, usesWwa is set if
// this app uses WWA, but not altered otherwise, so it will true if any of the
// inspected apps uses WWA.
bool inspectUwpAppManifest(const QString &installDir, AppExecutables &appExes);
