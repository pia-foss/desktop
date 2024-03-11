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
#include "win_com.h"
#include <kapps_core/src/stringslice.h>
#include <ShlObj.h>

namespace kapps { namespace core{

// Object that uses COM to read Windows shortcut files.  It's used when
// scanning apps and when loading app icons.
class KAPPS_CORE_EXPORT WinLinkReader
{
public:
    WinLinkReader();
private:
    WinLinkReader(const WinLinkReader &) = delete;
    WinLinkReader &operator=(const WinLinkReader &) = delete;

public:
    // Load a link from a file.  Returns false if the link can't be read.
    bool loadLink(const std::wstring &path);

    // Reads the link target from the currently loaded link
    std::wstring getLinkTarget(WStringSlice traceLinkPath);
    std::pair<std::wstring, int> getLinkIconLocation(WStringSlice traceLinkPath);
    // Get the length of the arguments specified in this link.  Returns
    // std::numeric_limits<std::size_t>::max() if the arguments can't be read.
    std::size_t getArgsLength(WStringSlice traceLinkPath);

private:
    WinComPtr<IShellLink> _pShellLink;
    // The IPersistFile interface to the same shell link object
    WinComPtr<IPersistFile> _pPersist;
};

// Expand environment variables in a path
KAPPS_CORE_EXPORT std::wstring expandEnvString(const wchar_t *pEnvStr);

}}
