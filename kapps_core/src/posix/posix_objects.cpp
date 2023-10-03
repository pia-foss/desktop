// Copyright (c) 2023 Private Internet Access, Inc.
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

#include "posix_objects.h"
#include "../logger.h"

#define _XOPEN_SOURCE 700   // Needed for SSIZE_MAX
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

namespace kapps { namespace core {

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

PosixFd& PosixFd::applyClOExec()
{
    applyFlags(F_GETFD, F_SETFD, FD_CLOEXEC);
    return *this;
}

PosixFd& PosixFd::applyNonblock()
{
    applyFlags(F_GETFL, F_SETFL, O_NONBLOCK);
    return *this;
}

bool PosixFd::appendAll(std::string &data)
{
    if(!*this)
        return false; // Not open, nothing to do

    while(true)
    {
        // If data exceeds the minimum chunk size, start doubling size to avoid
        // excessive recopying
        std::size_t chunkSize = std::max(std::size_t{4096}, data.size());
        // We can never read more than SSIZE_MAX at a time
        if(chunkSize > SSIZE_MAX)
            chunkSize = SSIZE_MAX;

        data.resize(data.size() + chunkSize);
        ssize_t nRead;
        NO_EINTR(nRead = ::read(get(), &*(data.end() - chunkSize), chunkSize));
        if(nRead < 0)
        {
            // Didn't get any data
            data.resize(data.size() - chunkSize);
            // EAGAIN is expected for a non-blocking socket.
            if(errno != EAGAIN)
            {
                KAPPS_CORE_WARNING() << "Read failed on fd" << get() << "-"
                    << ErrnoTracer{};
            }
            // Break out for any error - EAGAIN indicates there's no more data
            // right now, any other error probably cannot be retried
            break;
        }

        data.resize(data.size() - chunkSize + nRead);
        // If we read zero bytes, we have reached EOF.  This is specified by
        // POSIX (a zero-byte read from a non-blocking fd that is still
        // connected returns -1 with EAGAIN instead.)
        if(nRead == 0)
            return false;   // Reached EOF or remote hung up

        // If we read less than chunkSize, we're done, no more data right now
        if(static_cast<std::size_t>(nRead) < chunkSize)
            break;
        // Otherwise, we read chunkSize, repeat and allocate more space
    }

    return true;
}

bool PosixFd::discardAll()
{
    if(!*this)
        return false; // Not open, nothing to do

    std::array<char, 4096> discardData;
    while(true)
    {
        ssize_t nRead;
        NO_EINTR(nRead = ::read(get(), discardData.data(), discardData.size()));
        if(nRead < 0)
        {
            // EAGAIN is expected for a non-blocking socket.
            if(errno != EAGAIN)
            {
                KAPPS_CORE_WARNING() << "Discard failed on fd" << get() << "-"
                    << ErrnoTracer{};
            }
            break;
        }
        // If we read zero bytes, we have reached EOF.  This is specified by
        // POSIX (a zero-byte read from a non-blocking fd that is still
        // connected returns -1 with EAGAIN instead.)
        if(nRead == 0)
            return false;   // Reached EOF or remote hung up
        // If we read less than the chunk size, we're done, no more data now
        if(static_cast<std::size_t>(nRead) < discardData.size())
            break;
        // Otherwise, try to read more data
    }

    return true;
}

PosixPipe createPipe(bool cloexec)
{
    int pipefd[2]{PosixFd::Invalid, PosixFd::Invalid};

    // Pipe syscall is never interrupted by a signal, so don't need to wrap
    // with a NO_EINTR
    if(::pipe(pipefd)  < 0)
        throw std::runtime_error("Could not create pipe");

    // Own the new file descriptors
    PosixFd readEnd{pipefd[0]}, writeEnd{pipefd[1]};
    if(cloexec)
    {
        readEnd.applyClOExec();
        writeEnd.applyClOExec();
    }
    return {std::move(readEnd), std::move(writeEnd)};
}

std::pair<PosixFd, PosixFd> createSocketPair()
{
    int sockets[2]{PosixFd::Invalid, PosixFd::Invalid};
    auto spResult = ::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    if(spResult != 0)
        throw std::runtime_error("Could not create socketpair");

    PosixFd first{sockets[0]}, second{sockets[1]};
    first.applyClOExec();
    second.applyClOExec();

    return {std::move(first), std::move(second)};
}

}}
