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
#include <kapps_core/core.h>
#include "../util.h"
#include "posix_objects.h"
#include "../coresignal.h"
#include <poll.h>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>

namespace kapps { namespace core {

class KAPPS_CORE_EXPORT PollWorker;

// PollThread spawns a worker thread that waits on any number of POSIX file
// descriptors.  When a file descriptor is signaled, a functor is invoked with
// the file descriptor and the received events.
//
// Work items can also be enqueued to the worker from the main thread, like
// WorkThread.  Functors can also be enqueued synchronously or asynchronously
// using this mechanism.
class KAPPS_CORE_EXPORT PollThread
{
public:
    // PollThread's constructor creates the thread, which initially has no
    // file descriptors.  Specify the function used to handle work items.
    // Add file descriptors with addFds().
    PollThread(std::function<void(Any)> workFunc);

    // PollThread's destructor waits on the thread to exit.
    ~PollThread();

public:
    // Enqueue a work item to be handled by the worker thread.  Note that work
    // items are _not_ serialized with file descriptor events - if you both
    // enqueue() and then write(2) to a monitored file descriptor, those events
    // could be received in any order on the worker thread.
    //
    // This is thread-safe and can be invoked from any thread.
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
    // The work queue and the mutex used to control concurrent access.
    std::mutex _itemsMutex;
    std::queue<Any> _items;
    // When enqueuing a work item, this pipe is used to signal the poll thread
    // that work is available.  This is similar to the condition variable used
    // by WorkQueue.
    // When the pipe has data, the work thread drains the pipe and processes all
    // queued events.  This means that work items aren't serialized with other
    // file descriptor events, and that the work thread can wake even if it
    // already processed all work items, but those are both fine.
    PosixFd _itemSignalPipe;
    // The actual work thread.  The read end of the signal pipe and the
    // PollWorker are held on this thread.
    std::thread _workThread;
};


// A generic poll(2) worker - just keeps track of a set of file descriptors and
// their handlers for poll(2).  PollThread creates one of these on its worker
// thread, and the first file descriptor it provides is the work item signal
// pipe.
//
// Initially, PollWorker has no file descriptors - add them with addFd().
class PollWorker
{
public:
    // Add a file descriptor to monitor and its desired events.  This can be
    // called by the functor connected to activated().  (Note that PollWorker
    // itself is not thread-safe, to add a file descriptor for any thread, use
    // PollThread::addFd().)
    //
    // When the file descriptor receives events, activated() is invoked with
    // the token returned here.  Cancel the watch with removeFd() using the
    // token.
    int addFd(int fd, int events);

    // Remove a file descriptor.  Note that the file descriptor should still be
    // open at this point - if it's closed first, it could have already been
    // reused, and there may be a risk that someone else has already added it to
    // this PollWorker.
    void removeFd(int token);

    // Call poll(2) and invoke activated() for each triggered fd.
    // If there are no file descriptors yet, this will throw a
    // std::runtime_error.
    void pollFds();

public:
    Signal<int> activated; // Recieves watch token

private:
    // The file descriptors being monitored, their requested events, and
    // their received events - used as the pollfd vector for poll(2).  This
    // must be a vector because poll(2) requires contiguous storage.
    //
    // "Empty" entries with fd=-1 can occur here once file descriptors are
    // removed.  These are reused by addFd().  The vector never shrinks to
    // simplify pollFd(), which has to account for handlers possibly removing
    // file descriptors arbitrarily.
    std::vector<pollfd> _fds;
};

}}
