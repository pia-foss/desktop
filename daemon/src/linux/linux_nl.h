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

#ifndef LINUX_NL_H
#define LINUX_NL_H

#include "common.h"
#include "posix/posix_objects.h"
#include "networkmonitor.h"
#include <thread>
#include <future>

// Note: When working with libnl, it's useful to test with NLDBG=2 (or 1).
//
// libnl has useful diagnostic tracing that may point you toward subtle
// problems, particularly around some API requirements that are not clear (like
// the fact that a "route/addr" cache requires a provided "route/link" cache to
// work properly).

class LinuxNlCache;

// LinuxNl uses a netlink (via libnl) to retreive information about network
// addresses and wireless networks.
//
// The netlink sockets are serviced on a worker thread.  networksUpdated() is
// emitted on the main thread when new information is received (including
// initially when the state is first fetched).
class LinuxNl : public QObject
{
    Q_OBJECT

private:
    // An object of this class is created on the worker thread to operate the
    // caches.
    class Worker;

    // Worker thread procedure; operates the netlink sockets and queues calls to
    // networksUpdated() with updates.  Catches exceptions and gracefully
    // terminates the thread.
    static void runOnWorkerThread(LinuxNl *pThis,
                                  std::promise<PosixFd> killSocketPromise);

public:
    // The constructor starts the worker thread and reads the initial state.
    //
    // A networksUpdated() signal will be emitted later on the main thread with
    // that initial state - it's queued to the event loop, so connections to
    // networksUpdated() can be made safely after constructing LinuxNl before
    // returning control to the main event loop.
    LinuxNl();
    ~LinuxNl();

signals:
    // This signal is emitted on the main thread when new connection info is
    // available (including when the cache is first loaded, as long as the
    // cache is set up successfully).
    void networksUpdated(const std::vector<NetworkConnection> &connections);

private:
    std::thread _workerThread;
    // The socket used to terminate the worker thread.  The worker thread
    // procedure waits on its connected socket in addition to the netlink
    // sockets.  This is provided by the worker thread using socketpair().
    // If the worker thread couldn't be initialized, it returns a socket handle
    // of -1.
    std::future<PosixFd> _workerKillSocket;
};

#endif
