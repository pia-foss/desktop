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
#line SOURCE_FILE("win_linkreader.cpp")

#include "win_linkreader.h"
#include "win_util.h"
#include <ShlObj.h>
#include <shellapi.h>
#include <array>

WinLinkReader::WinLinkReader()
{
    _pShellLink = WinComPtr<IShellLink>::createInprocInst(CLSID_ShellLink,
        IID_IShellLink);
    _pPersist = _pShellLink.queryInterface<IPersistFile>(IID_IPersistFile);

    // If either failed, we can't create this object.
    if(!_pShellLink || !_pPersist)
    {
        qWarning() << "Can't load shell link COM object";
        throw Error{HERE, Error::Code::Unknown};
    }
}

bool WinLinkReader::loadLink(const QString &path)
{
    Q_ASSERT(_pShellLink);  // Class invariant
    Q_ASSERT(_pPersist);  // Class invariant

    // If we're unable to read an icon location from the link for any
    // reason, fall back to loading an icon from the link target.
    HRESULT loadErr = _pPersist->Load(qstringWBuf(path), STGM_READ|STGM_SHARE_DENY_WRITE);
    if(FAILED(loadErr))
    {
        qInfo() << "Unable to read link" << path << "-" << loadErr;
        return false;
    }

    return true;
}

std::wstring WinLinkReader::getLinkTarget(const QStringView &traceLinkPath)
{
    std::wstring targetPath;
    targetPath.resize(MAX_PATH);
    HRESULT pathErr = _pShellLink->GetPath(targetPath.data(),
        targetPath.size(), nullptr, 0);
    if(FAILED(pathErr))
    {
        qInfo() << "Unable to read link target" << traceLinkPath << "-" << pathErr;
        return {};
    }

    if(pathErr == S_FALSE)
    {
        qInfo() << "Link does not point to a file:" << traceLinkPath;
        return {};
    }

    targetPath.resize(std::wcslen(targetPath.c_str()));
    return targetPath;
}

std::pair<std::wstring, int> WinLinkReader::getLinkIconLocation(const QStringView &traceLinkPath)
{
    int iconIdx{-1};

    Q_ASSERT(_pShellLink);  // Class invariant
    Q_ASSERT(_pPersist);  // Class invariant

    std::array<wchar_t, MAX_PATH> iconUnexpPath;
    HRESULT locErr = _pShellLink->GetIconLocation(iconUnexpPath.data(),
                                                  iconUnexpPath.size(),
                                                  &iconIdx);
    if(FAILED(locErr))
    {
        qInfo() << "Unable to get icon location for" << traceLinkPath << "-" << locErr;
        return {{}, 0};
    }

    if(!iconUnexpPath[0])
        return {{}, 0}; // Empty path.  This is common, don't trace it.

    std::wstring iconPath = expandEnvString(iconUnexpPath.data());
    if(iconPath.empty())
    {
        qWarning() << "Couldn't expand module path" << QStringView{iconUnexpPath.data()};
        return {{}, 0};
    }

    return {std::move(iconPath), static_cast<WORD>(iconIdx)};
}

std::size_t WinLinkReader::getArgsLength(const QStringView &traceLinkPath)
{
    Q_ASSERT(_pShellLink);  // Class invariant

    std::array<wchar_t, MAX_PATH> args;
    HRESULT argsResult = _pShellLink->GetArguments(args.data(), args.size());
    if(FAILED(argsResult))
    {
        qWarning() << "Can't get arguments from link" << traceLinkPath << "-"
            << argsResult;
        return std::numeric_limits<std::size_t>::max();
    }

    // GetArguments() can silently truncate the string, but that's OK for the
    // app list where this is used, we'll just consider all "very long" argument
    // strings the same length.
    return ::wcslen(args.data());
}
