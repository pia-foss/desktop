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

#include "win_linkreader.h"
#include <kapps_core/src/logger.h>
#include <ShlObj.h>
#include <shellapi.h>
#include <array>

namespace kapps { namespace core {

WinLinkReader::WinLinkReader()
{
    _pShellLink = WinComPtr<IShellLink>::createInprocInst(CLSID_ShellLink,
        IID_IShellLink);
    _pPersist = _pShellLink.queryInterface<IPersistFile>(IID_IPersistFile);

    // If either failed, we can't create this object.
    if(!_pShellLink || !_pPersist)
    {
        KAPPS_CORE_WARNING() << "Can't load shell link COM object";
        // TODO - Move base of Error to kapps::core
        throw std::runtime_error{"Can't load shell link COM object"};
        //throw Error{HERE, Error::Code::Unknown};
    }
}

bool WinLinkReader::loadLink(const std::wstring &path)
{
    assert(_pShellLink);  // Class invariant
    assert(_pPersist);  // Class invariant

    // If we're unable to read an icon location from the link for any
    // reason, fall back to loading an icon from the link target.
    HRESULT loadErr = _pPersist->Load(path.c_str(), STGM_READ|STGM_SHARE_DENY_WRITE);
    if(FAILED(loadErr))
    {
        KAPPS_CORE_INFO() << "Unable to read link" << path << "-" << loadErr;
        return false;
    }

    return true;
}

std::wstring WinLinkReader::getLinkTarget(WStringSlice traceLinkPath)
{
    std::wstring targetPath;
    targetPath.resize(MAX_PATH);
    HRESULT pathErr = _pShellLink->GetPath(targetPath.data(),
        targetPath.size(), nullptr, 0);
    if(FAILED(pathErr))
    {
        KAPPS_CORE_INFO() << "Unable to read link target" << traceLinkPath << "-" << pathErr;
        return {};
    }

    if(pathErr == S_FALSE)
    {
        KAPPS_CORE_INFO() << "Link does not point to a file:" << traceLinkPath;
        return {};
    }

    targetPath.resize(std::wcslen(targetPath.c_str()));
    return targetPath;
}

std::pair<std::wstring, int> WinLinkReader::getLinkIconLocation(WStringSlice traceLinkPath)
{
    int iconIdx{-1};

    assert(_pShellLink);  // Class invariant
    assert(_pPersist);  // Class invariant

    std::array<wchar_t, MAX_PATH> iconUnexpPath;
    HRESULT locErr = _pShellLink->GetIconLocation(iconUnexpPath.data(),
                                                  iconUnexpPath.size(),
                                                  &iconIdx);
    if(FAILED(locErr))
    {
        KAPPS_CORE_INFO() << "Unable to get icon location for" << traceLinkPath << "-" << locErr;
        return {{}, 0};
    }

    if(!iconUnexpPath[0])
        return {{}, 0}; // Empty path.  This is common, don't trace it.

    std::wstring iconPath = expandEnvString(iconUnexpPath.data());
    if(iconPath.empty())
    {
        KAPPS_CORE_WARNING() << "Couldn't expand module path" << WStringSlice{iconUnexpPath.data()};
        return {{}, 0};
    }

    return {std::move(iconPath), static_cast<WORD>(iconIdx)};
}

std::size_t WinLinkReader::getArgsLength(WStringSlice traceLinkPath)
{
    assert(_pShellLink);  // Class invariant

    std::array<wchar_t, MAX_PATH> args;
    HRESULT argsResult = _pShellLink->GetArguments(args.data(), args.size());
    if(FAILED(argsResult))
    {
        KAPPS_CORE_WARNING() << "Can't get arguments from link" << traceLinkPath << "-"
            << argsResult;
        return std::numeric_limits<std::size_t>::max();
    }

    // GetArguments() can silently truncate the string, but that's OK for the
    // app list where this is used, we'll just consider all "very long" argument
    // strings the same length.
    return ::wcslen(args.data());
}

std::wstring expandEnvString(const wchar_t *pEnvStr)
{
    std::wstring expanded;
    expanded.resize(MAX_PATH);
    auto len = ::ExpandEnvironmentStringsW(pEnvStr, expanded.data(),
                                           expanded.size());
    if(len < 0 || len > expanded.size())
        return {};
    expanded.resize(len-1); // len includes the terminating null char
    return expanded;
}

}}
