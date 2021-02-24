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
#line HEADER_FILE("win_linkreader.h")

#ifndef WIN_LINKREADER_H
#define WIN_LINKREADER_H

#include "win_com.h"
#include <QFileInfo>
#include <ShlObj.h>

// Object that uses COM to read Windows shortcut files.  It's used when
// scanning apps and when loading app icons.
class COMMON_EXPORT WinLinkReader
{
public:
    WinLinkReader();
private:
    WinLinkReader(const WinLinkReader &) = delete;
    WinLinkReader &operator=(const WinLinkReader &) = delete;

public:
    // Load a link from a file.  Returns false if the link can't be read.
    bool loadLink(const QString &path);

    // Reads the link target from the currently loaded link
    std::wstring getLinkTarget(const QStringView &traceLinkPath);
    std::pair<std::wstring, int> getLinkIconLocation(const QStringView &traceLinkPath);
    // Get the length of the arguments specified in this link.  Returns
    // std::numeric_limits<std::size_t>::max() if the arguments can't be read.
    std::size_t getArgsLength(const QStringView &traceLinkPath);

private:
    WinComPtr<IShellLink> _pShellLink;
    // The IPersistFile interface to the same shell link object
    WinComPtr<IPersistFile> _pPersist;
};

#endif
