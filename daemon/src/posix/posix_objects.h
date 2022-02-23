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

#include "common.h"
#line HEADER_FILE("posix_objects.h")

#ifndef POSIX_OBJECTS_H
#define POSIX_OBJECTS_H

// File descriptor handle owner; closes the descriptor with ::close().
class PosixFd
{
public:
    enum : int {Invalid = -1};

public:
    PosixFd() : _file{Invalid} {};
    explicit PosixFd(int file) : _file{file} {}
    // Movable but not copiable
    PosixFd(PosixFd &&other) : PosixFd{} {*this = std::move(other);}
    PosixFd &operator=(PosixFd &&other) {std::swap(_file, other._file); return *this;}
    ~PosixFd();

private:
    // Apply descriptor flags or file status flags - the get/set fcntl commands
    // are either F_GETFD/F_SETFD or F_GETFL/F_SETFL.
    // No effect if the file descriptor is invalid.
    void applyFlags(int getCmd, int setCmd, int newFlags);

public:
    explicit operator bool() const {return _file != Invalid;}
    bool operator!() const {return !this->operator bool();}
    int get() const {return _file;}

    // Set FD_CLOEXEC on this file descriptor
    void applyClOExec();
    // Set O_NONBLOCK on this file decriptor
    void applyNonblock();

private:
    int _file;
};

// Create a socket pair, returned as two PosixFds.  FD_CLOEXEC is set on both
// PosixFds.
std::pair<PosixFd, PosixFd> createSocketPair();

#endif
