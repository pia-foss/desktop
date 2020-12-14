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
#line HEADER_FILE("mac/utun.h")

#ifndef UTUN_H
#define UTUN_H

class UTun
{
public:
    UTun()
        : _fd{-1}
        , _unitNumber{-1}
    {}

    UTun(UTun &&other)
    : UTun{}
    {
        *this = std::move(other);
    }

    ~UTun() { if(_fd > 0) ::close(_fd); }
    UTun &operator=(UTun &&other)
    {
        std::swap(_fd, other._fd);
        std::swap(_unitNumber, other._unitNumber);
        return *this;
    }

    int open(int unitNumber);
    void close();
    int unitNumber() const { return _unitNumber; }
    QString name() const { return QStringLiteral("utun%1").arg(_unitNumber - 1); }
    int fd() const { return _fd; }
    bool isOpen() const { return _fd > 0; }
    uint setMtu(uint mtu);
    uint mtu() const;

    static UTun create();
private:
    int _fd;
    int _unitNumber;
};

#endif
