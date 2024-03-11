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
#include "../util.h"
#include <kapps_core/core.h>
#include <utility>

namespace kapps { namespace core {

// File descriptor handle owner; closes the descriptor with ::close().
class KAPPS_CORE_EXPORT PosixFd
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
    PosixFd& applyClOExec();
    // Set O_NONBLOCK on this file decriptor
    PosixFd& applyNonblock();

    void close() { *this = PosixFd{}; }

    // Read and append all available data to the std::string specified.
    // For nonblocking file descriptors, this stops if EAGAIN occurs - only
    // currently available data are read.
    //
    // The result indicates whether the file descriptor is still connected and
    // readable (true) or has reached EOF/the remote has hung up (false).
    //
    // If the PosixFd is not open, does nothing and returns false (not open).
    bool appendAll(std::string &data);
    // Read all available data into a std::string - shortcut for calling
    // appendAll() with an empty string.  Note that it is not possible to
    // observe whether this reaches EOF.
    std::string readAll() {std::string data; appendAll(data); return data;}

    // Discard all available data.  Like appendAll(), stops if EAGAIN occurs.
    // This works by reading chunks of data and ignoring them; it is suitable
    // for non-seekable file descriptors like pipes.  The return is the same as
    // appendAll(); indicates whether the file is still connected and readable
    // (true) or has reached EOF/the remote has hung up (false).
    bool discardAll();

private:
    int _file;
};

struct PosixPipe
{
    PosixFd readEnd;
    PosixFd writeEnd;
};

// Create a pipe, returned as two PosixFds.  FD_CLOEXEC is set on both by
// default, which is almost always needed - cloexec=false prevents this; used
// by Process to set up the child's stdout/stderr, where the write ends do need
// to survive exec().
PosixPipe KAPPS_CORE_EXPORT createPipe(bool cloexec = true);

// Create a socket pair, returned as two PosixFds.  FD_CLOEXEC is set on both
// PosixFds.
std::pair<PosixFd, PosixFd> KAPPS_CORE_EXPORT createSocketPair();

}}
