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
#line HEADER_FILE("linux_objects.h")

#ifndef LINUX_OBJECTS_H
#define LINUX_OBJECTS_H

// File descriptor handle owner; closes the descriptor with ::close().
class LinuxFd
{
public:
    enum : int {Invalid = -1};

public:
    LinuxFd() : _file{Invalid} {};
    explicit LinuxFd(int file) : _file{file} {}
    // Movable but not copiable
    LinuxFd(LinuxFd &&other) : LinuxFd{} {*this = std::move(other);}
    LinuxFd &operator=(LinuxFd &&other) {std::swap(_file, other._file); return *this;}
    ~LinuxFd();

public:
    explicit operator bool() const {return _file != Invalid;}
    bool operator!() const {return !this->operator bool();}
    int get() const {return _file;}

private:
    int _file;
};

#endif
