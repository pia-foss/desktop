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
#include <kapps_core/core.h>
#include "winapi.h"

// TODO - kapps::core namespace

// Generic handle owner.  Closer_t::operator() is used to close the handle.
// The closer is specified this way rather than specializing a separate function
// for various handle types, because many of the handle types are all typedefs
// of "void *" but stll require different close functions.
template<class Handle_t, class Closer_t>
class WinGenericHandle
{
public:
    WinGenericHandle() : WinGenericHandle{nullptr} {}
    explicit WinGenericHandle(Handle_t handle) : _handle{handle} {}
    WinGenericHandle(WinGenericHandle &&other) : WinGenericHandle{} {*this = std::move(other);}
    ~WinGenericHandle()
    {
        if(_handle)
            Closer_t{}(_handle);
    }
    WinGenericHandle &operator=(WinGenericHandle &&other)
    {
        std::swap(_handle, other._handle);
        return *this;
    }

public:
    explicit operator bool() const {return _handle;}
    operator Handle_t() const {return _handle;}
    Handle_t get() const {return _handle;}
    Handle_t *receive() {*this = {}; return &_handle;}
    void swap(WinGenericHandle &other) {std::swap(_handle, other._handle);}

private:
    Handle_t _handle;
};

struct KAPPS_CORE_EXPORT WinCloseHandle
{
    void operator()(HANDLE handle){::CloseHandle(handle);}
};

struct KAPPS_CORE_EXPORT WinCloseHKey
{
    void operator()(HKEY handle){::RegCloseKey(handle);}
};

// HANDLE owner using ::CloseHandle()
using WinHandle = WinGenericHandle<HANDLE, WinCloseHandle>;
// HKEY owner using ::RegCloseKey()
using WinHKey = WinGenericHandle<HKEY, WinCloseHKey>;
