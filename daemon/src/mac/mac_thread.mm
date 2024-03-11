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

#include <common/src/common.h>
#line SOURCE_FILE("mac_thread.mm")

#include "mac_thread.h"

void MacRunLoopThread::runOnWorkerThread(std::promise<CFHandle<CFRunLoopRef>> runLoopPromise,
                                         CFHandle<CFRunLoopSourceRef> source)
{
    // Grab a reference to this thread's run loop (otherwise it may or may not
    // be destroyed after CFRunLoopRun() returns)
    CFHandle<CFRunLoopRef> thisRunLoop{true, ::CFRunLoopGetCurrent()};

    // Provide the run loop handle back to the main thread
    runLoopPromise.set_value(thisRunLoop);

    // Add the event source
    ::CFRunLoopAddSource(thisRunLoop.get(), source.get(), kCFRunLoopDefaultMode);

    // Run the event loop until stopped by the destructor
    ::CFRunLoopRun();

    // Remove the event source
    ::CFRunLoopRemoveSource(thisRunLoop.get(), source.get(), kCFRunLoopDefaultMode);
}

MacRunLoopThread::MacRunLoopThread(CFHandle<CFRunLoopSourceRef> source)
{
    // Create the promise and future for the worker thread run loop
    std::promise<CFHandle<CFRunLoopRef>> runLoopPromise;
    _runLoopFuture = runLoopPromise.get_future();

    // Start the worker thread
    _workerThread = std::thread{&MacRunLoopThread::runOnWorkerThread,
                                std::move(runLoopPromise),
                                std::move(source)};
}

MacRunLoopThread::~MacRunLoopThread()
{
    // Terminate the run loop.  If the worker thread is still starting,
    // std::future::get() blocks until it provides its run loop handle.
    ::CFRunLoopStop(_runLoopFuture.get().get());

    // Wait for the thread to exit.
    _workerThread.join();
}
