// Copyright (c) 2020 Private Internet Access, Inc.
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
#line SOURCE_FILE("win_util.cpp")

#include "win_util.h"
#include <Windows.h>
#include <QDebug>

#pragma comment(lib, "User32.lib")

const wchar_t *qstringWBuf(const QString &value)
{
    // QString::utf16() returns unsigned shorts; on Windows this is the same as
    // a wchar_t array.  The string returned by utf16() is null-terminated.
    static_assert(sizeof(ushort) == sizeof(wchar_t), "QString-LPCWSTR conversion assumes wchar_t==ushort");
    return reinterpret_cast<const wchar_t*>(value.utf16());
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

ProcAddress::ProcAddress(const QString &module, const QByteArray &entrypoint)
    : _moduleHandle{nullptr}, _procAddress{nullptr}
{
    _moduleHandle = ::LoadLibraryW(qstringWBuf(module));
    if(_moduleHandle)
        _procAddress = ::GetProcAddress(_moduleHandle, entrypoint.data());
}

ProcAddress::~ProcAddress()
{
    if(_moduleHandle)
        ::FreeLibrary(_moduleHandle);
}

void broadcastMessage(const LPCWSTR &message)
{
    UINT msg = ::RegisterWindowMessageW(message);
    if(!msg)
    {
        qWarning() << "Unable to register desired message for broadcast - error"
            << ::GetLastError();
    }
    else
    {
        qDebug () << "Broadcasting message " << QString::fromWCharArray(message);
        PostMessage(HWND_BROADCAST, msg, 0, 0);
    }
}
