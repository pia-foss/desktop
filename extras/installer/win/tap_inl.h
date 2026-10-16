// Copyright (c) 2026 Private Internet Access, Inc.
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
// DriverStatus and WinErrorEx are shared by all the driver install tasks, and
// the callout driver API is declared in callout_inl.h.
#include "common_inl.h"
#include "callout_inl.h"
#include "service_inl.h"
#include <string>
#include <Windows.h>

class WinDriverVersion
{
public:
    WinDriverVersion() : WinDriverVersion{0, 0, 0, 0} {}
    WinDriverVersion(WORD v0, WORD v1, WORD v2, WORD v3) : _v0{v0}, _v1{v1}, _v2{v2}, _v3{v3} {}
    WinDriverVersion(DWORDLONG version)
    {
        ULARGE_INTEGER versionUli;
        versionUli.QuadPart = version;
        _v0 = HIWORD(versionUli.HighPart);
        _v1 = LOWORD(versionUli.HighPart);
        _v2 = HIWORD(versionUli.LowPart);
        _v3 = LOWORD(versionUli.LowPart);
    }

public:
    bool operator==(const WinDriverVersion &other) const
    {
        return _v0 == other._v0 && _v1 == other._v1 && _v2 == other._v2 && _v3 == other._v3;
    }
    bool operator!=(const WinDriverVersion &other) const {return !(*this == other);}
    bool operator<(const WinDriverVersion &other) const
    {
        if(_v0 < other._v0)
            return true;
        if(_v1 < other._v1)
            return true;
        if(_v2 < other._v2)
            return true;
        return _v3 < other._v3;
    }
    bool operator>(const WinDriverVersion &other) const {return other < *this;}
    bool operator<=(const WinDriverVersion &other) const {return !(*this > other);}
    bool operator>=(const WinDriverVersion &other) const {return !(*this < other);}

    std::string printable() const
    {
        char buf[24];
        _snprintf(buf, sizeof(buf), "%hu.%hu.%hu.%hu", _v0, _v1, _v2, _v3);
        return {buf};
    }

private:
    WORD _v0, _v1, _v2, _v3;
};

DriverStatus installTapDriver(LPCWSTR inf, bool alwaysCreateNew, bool forceUpdate, bool nonInteractive = false);
bool uninstallTapDriverInf();
DriverStatus uninstallTapDriver(bool removeInf, bool onlyDifferentVersion);
