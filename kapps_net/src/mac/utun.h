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
#include <kapps_net/net.h>
#include <kapps_core/src/util.h>
#include <kapps_core/src/posix/posix_objects.h>

class KAPPS_NET_EXPORT UTun
{
public:
    int fd() const {return _sock.get();}
    int setMtu(int mtu);
    int mtu() const;
    const std::string& name() const {return _name;}
    static kapps::core::nullable_t<UTun> create();
private:
    // Users must use UTun::create()
    UTun() = default;
    explicit UTun(int unitNumber);
    bool valid() const {return _unitNumber != Invalid;}
private:
    enum : int { Invalid = -1 };
    // Since this member is move-only we can rely on rule of 0
    // to generate only move ctor/move assignment but NOT
    // copy ctors/assignment
    kapps::core::PosixFd _sock;
    int _unitNumber{Invalid};
    std::string _name;
};
