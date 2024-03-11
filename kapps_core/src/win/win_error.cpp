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

#include "win_error.h"
namespace kapps { namespace core {

std::wstring WinErrTracer::message() const
{
    LPWSTR errMsg{nullptr};

    auto len = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                                nullptr, code(), 0,
                                reinterpret_cast<LPWSTR>(&errMsg), 0, nullptr);
    std::wstring msg{errMsg, len};
    ::LocalFree(errMsg);

    return msg;
}

std::ostream &operator<<(std::ostream &os, const WinErrTracer &err)
{
    return os << err.code() << ' ' << WStringSlice{err.message()};
}

}}
