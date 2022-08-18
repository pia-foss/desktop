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

#include "appidkey.h"
#include <kapps_core/src/stringslice.h>
#include <kapps_core/src/logger.h>

namespace kapps { namespace net {

bool AppIdKey::operator==(const AppIdKey &other) const
{
    if(!_pBlob && !other._pBlob)
        return true;    // Both empty
    if(!_pBlob || !other._pBlob)
        return false;   // One is empty but the other isn't

    return std::equal(_pBlob->data, _pBlob->data + _pBlob->size,
                      other._pBlob->data, other._pBlob->data + other._pBlob->size);
}

bool AppIdKey::operator<(const AppIdKey &other) const
{
    if(!_pBlob && !other._pBlob)
        return false;   // Both empty - equal
    // Empties are less than non-empties
    if(!_pBlob)
        return true; // This is empty
    if(!other._pBlob)
        return false; // Other is empty

    return std::lexicographical_compare(_pBlob->data, _pBlob->data + _pBlob->size,
                                        other._pBlob->data, other._pBlob->data + other._pBlob->size);
}

bool AppIdKey::operator==(const std::vector<char> &value) const
{
    if(!_pBlob)
        return value.empty(); // Empty blob - equal if the value is also empty
    return std::equal(_pBlob->data, _pBlob->data + _pBlob->size,
                      value.begin(), value.end());
}

void AppIdKey::clear()
{
    if(_pBlob)
    {
        ::FwpmFreeMemory(reinterpret_cast<void**>(&_pBlob));
        _pBlob = nullptr;
    }
}

void AppIdKey::reset(const std::wstring &appPath)
{
    clear();

    if(DWORD error = FwpmGetAppIdFromFileName(appPath.c_str(), &_pBlob))
    {
        KAPPS_CORE_WARNING() << "Can't get app ID for" << appPath << "-"
            << core::WinErrTracer{error};
        _pBlob = nullptr;
        // Rely on the filter addition failing later
    }
    else if(_pBlob)
    {
        KAPPS_CORE_INFO() << "Got app ID for" << appPath;
    }
}

std::vector<char> AppIdKey::copyData() const
{
    // char is 8 bits on all Windows platforms
    static_assert(sizeof(char) == sizeof(_pBlob->data[0]));

    if(!_pBlob)
        return {};
    const char *pDataBegin = reinterpret_cast<const char*>(_pBlob->data);
    return std::vector<char>{pDataBegin, pDataBegin + _pBlob->size};
}

core::WStringSlice AppIdKey::printableString() const
{
    if(!_pBlob)
        return L"<null>";

    core::WStringSlice printable{reinterpret_cast<const wchar_t *>(_pBlob->data),
                                 static_cast<std::size_t>(_pBlob->size / sizeof(wchar_t))};

    auto truncateLen = printable.size();
    while(truncateLen > 0 && printable[truncateLen-1] == L'\0')
        --truncateLen;
    return {printable.data(), truncateLen};
}

void AppIdKey::trace(std::ostream &os) const
{
    os << printableString();
}

}}
