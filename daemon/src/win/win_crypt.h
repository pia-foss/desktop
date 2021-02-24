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
#line HEADER_FILE("win/win_crypt.h")

#ifndef WIN_CRYPT_H
#define WIN_CRYPT_H

#include <string>
#include <set>

// Get the subject names from signatures in an executable.
// This does _not_ verify the signatures, it's used by WinAppMonitor to try to
// figure out if executables might belong to the same app - the executables are
// already running, so there's no point in actually verifying the signatures.
std::set<std::wstring> winGetExecutableSigners(const QStringView &path);

#endif
