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
#line HEADER_FILE("win_util.h")

#ifndef WIN_UTIL_H
#define WIN_UTIL_H

#include <QString>
#include <Windows.h>
#include <string>

// On Windows, get a wchar_t pointer from a QString.
COMMON_EXPORT const wchar_t *qstringWBuf(const QString &value);

// Expand environment variables in a path
COMMON_EXPORT std::wstring expandEnvString(const wchar_t *pEnvStr);

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

private:
    Handle_t _handle;
};

struct COMMON_EXPORT WinCloseHandle
{
    void operator()(HANDLE handle){::CloseHandle(handle);}
};

// HANDLE owner using ::CloseHandle()
using WinHandle = WinGenericHandle<HANDLE, WinCloseHandle>;

// ProcAddress wraps up an HMODULE and a function pointer retrieved with
// ::GetProcAddress().  Like ::GetProcAddress(), it can be used to call an API
// conditionally if it is found in a module, such as APIs added in recent
// versions of Windows.
class COMMON_EXPORT ProcAddress
{
public:
    ProcAddress() : _moduleHandle{}, _procAddress{} {}
    ProcAddress(const QString &module, const QByteArray &entrypoint);
    ProcAddress(ProcAddress &&other) : ProcAddress{} {*this = std::move(other);}
    ~ProcAddress();

    ProcAddress &operator=(ProcAddress &&other)
    {
        std::swap(_moduleHandle, other._moduleHandle);
        std::swap(_procAddress, other._procAddress);
        return *this;
    }

private:
    ProcAddress(const ProcAddress &) = delete;
    ProcAddress &operator=(const ProcAddress &) = delete;

public:
    void *get() const {return _procAddress;}

private:
    HMODULE _moduleHandle;
    void *_procAddress;
};

// Broadcast a registered window message.  Since this registers the message each
// time it's called, this should generally be used for messages that are only
// occasionally sent.
COMMON_EXPORT void broadcastMessage(const LPCWSTR &message);

#endif
