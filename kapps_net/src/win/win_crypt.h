// Copyright (c) 2024 Private Internet Access, Inc.
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
#include <string>
#include <set>

namespace kapps { namespace net {

// Get the subject names from signatures in an executable.
// This does _not_ verify the signatures, it's used by WinAppMonitor to try to
// figure out if executables might belong to the same app - the executables are
// already running, so there's no point in actually verifying the signatures.
std::set<std::wstring> winGetExecutableSigners(const std::wstring &path);

}}
