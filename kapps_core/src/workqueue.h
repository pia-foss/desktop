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

#pragma once
#include <kapps_core/core.h>
#include "util.h"
#include <condition_variable>
#include <queue>
#include <functional>
#include <thread>

namespace kapps { namespace core {

// WorkQueue is a thread-safe work item queue.  One or more threads can push
// work items into the queue (of any type), and other threads can wait on them.
//
// The work items are held in Any; they do not need to be copiable.  If a
// WorkQueue is destroyed while holding unprocessed items, the items are
// destroyed correctly.  (Be sure to use proper owning types in work items to
// ensure that they clean up in this case; avoid queueing raw HANDLE, void*,
// etc.)
class KAPPS_CORE_EXPORT WorkQueue
{
public:
    // Enqueue a new work item.
    void enqueue(Any item);
    
    // Dequeue the next work item - wait until one is queued if necessary.
    Any dequeue();

private:
    std::condition_variable _haveItems;
    // _itemsMutex protects _items only.
    std::mutex _itemsMutex;
    std::queue<Any> _items;
};

// WorkThread creates a worker thread that waits to process items passed through
// a WorkQueue.  The constructor starts the thread, and the work item functor is
// invoked for each work item.  The destructor shuts down the thread, including
// waiting for it to exit.
class KAPPS_CORE_EXPORT WorkThread
{
private:
    // Value passed to tell the work thread to exit.
    struct Terminate{};

public:
    WorkThread(std::function<void(Any)> workFunc);
    ~WorkThread();
    
private:
    // Thread procedure used on the worker thread - pumps the WorkQueue until a
    // Terminate is received.  workFunc is captured in a lambda created by the
    // constructor.
    void workThreadProc(const std::function<void(Any)> &workFunc);
    
public:
    void enqueue(Any item);
    
    // Queue a functor to be invoked on the work thread asynchronously.
    // (This just enqueue()s a WorkFunc containing the functor given.)
    void queueInvoke(std::function<void()> func);
    
    // Invoke a functor synchronously.  The calling thread is blocked until the
    // work thread has picked up the functor and invoked it.  This functor can
    // safely capture references to data in the calling thread.
    //
    // If the functor throws an exception, it's re-thrown on this thread.
    void syncInvoke(std::function<void()> func);

private:
    // The work queue used to pass work items and the Terminate object.
    WorkQueue _queue;
    std::thread _workThread;
};

}}
