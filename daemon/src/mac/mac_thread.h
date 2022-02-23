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
#line HEADER_FILE("mac_thread.h")

#ifndef MAC_THREAD_H
#define MAC_THREAD_H

#include "mac_objects.h"
#include <CoreFoundation/CoreFoundation.h>
#include <thread>
#include <future>

// MacRunLoopThread creates a thread that runs a CFRunLoop to service a given
// run loop source.  (Qt doesn't use a CFRunLoop for its event loop, so a worker
// thread is needed to handle notifications based on a run loop source.)
//
// Currently this just services a single run loop source, but in principle it
// could be extended to multiple sources (to avoid creating excessive threads
// that are mostly idle).
class MacRunLoopThread
{
public:
    // Create a run loop thread to service this source.  MacRunLoopThread
    // retains a reference to the source in order to remove it when the thread
    // exits.
    //
    // Note that callbacks invoked by the event source will be invoked on this
    // thread, so they'll typically have to dispatch back to the main thread.
    MacRunLoopThread(CFHandle<CFRunLoopSourceRef> source);

    // The destructor stops the run loop thread and waits for it to terminate.
    ~MacRunLoopThread();

    // Run the event loop on the worker thread (used as the thread procedure).
    static void runOnWorkerThread(std::promise<CFHandle<CFRunLoopRef>> runLoopPromise,
                                  CFHandle<CFRunLoopSourceRef> source);

private:
    MacRunLoopThread(const MacRunLoopThread &) = delete;
    MacRunLoopThread &operator=(const MacRunLoopThread &) = delete;

private:
    std::thread _workerThread;
    // The run loop for the worker thread.  This is provided by the worker
    // thread when it starts, then is used on the main thread to stop the worker
    // thread's run loop (which causes the thread to exit).
    std::future<CFHandle<CFRunLoopRef>> _runLoopFuture;
};

#endif
