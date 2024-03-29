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
#include <kapps_core/core.h>
#include "../stringslice.h"
#include "winapi.h"

namespace kapps { namespace core {

class KAPPS_CORE_EXPORT WinErrTracer
{
public:
    WinErrTracer(DWORD code) : _code{code} {}

public:
    DWORD code() const {return _code;}
    std::wstring message() const;

private:
    DWORD _code;
};

KAPPS_CORE_EXPORT std::ostream &operator<<(std::ostream &os, const WinErrTracer &err);

}}
