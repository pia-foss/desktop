// Copyright (c) 2021 Private Internet Access, Inc.
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
#line SOURCE_FILE("posix_objects.cpp")

#include "posix_objects.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

PosixFd::~PosixFd()
{
    if(*this)
        ::close(_file);
}

void PosixFd::applyFlags(int getCmd, int setCmd, int newFlags)
{
    if(!*this)
        return;

    int oldFlags = ::fcntl(get(), getCmd);
    ::fcntl(get(), setCmd, oldFlags | newFlags);
}

void PosixFd::applyClOExec()
{
    applyFlags(F_GETFD, F_SETFD, FD_CLOEXEC);
}

void PosixFd::applyNonblock()
{
    applyFlags(F_GETFL, F_SETFL, O_NONBLOCK);
}

std::pair<PosixFd, PosixFd> createSocketPair()
{
    int sockets[2]{PosixFd::Invalid, PosixFd::Invalid};
    auto spResult = ::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    if(spResult != 0)
    {
        qWarning() << "Failed to create socketpair:" << ErrnoTracer{};
        return {};
    }

    PosixFd first{sockets[0]}, second{sockets[1]};
    first.applyClOExec();
    second.applyClOExec();

    return {std::move(first), std::move(second)};
}
