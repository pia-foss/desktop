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

#include "pollthread.h"
#include "../logger.h"
#include "../workfunc.h"
#include "posixfdnotifier.h"
#include <cassert>
#include <unistd.h>

namespace kapps { namespace core {

// The thread created by PollThread provides an EventLoop implementation using
// its PollWorker.  Timers are currently not implemented.
class PollThreadEventLoop : public EventLoop
{
public:
    PollThreadEventLoop(PollWorker &worker)
        : _worker{worker}
    {
        _worker.activated = [this](int token)
        {
            fdWatchTriggered(token);
        };
    }

public:
    virtual TokenT setTimer(std::chrono::milliseconds, bool) override
    {
        // Not implemented
        return InvalidToken;
    }
    virtual void cancelTimer(TokenT) override {}
    virtual TokenT watchFd(int fd, WatchType type) override
    {
        int events = (type == WatchType::Write) ? POLLOUT|POLLHUP : POLLIN|POLLHUP;
        return _worker.addFd(fd, events);
    }
    virtual void cancelFdWatch(TokenT token) override
    {
        return _worker.removeFd(token);
    }

private:
    // The poll worker used to implement this event loop.
    PollWorker &_worker;
};

// PollThread's special flavor of PollWorker.  This adds a loop arond poll(2), a
// continue flag, handler function for work items, and the user's work function.
class PollThreadWorker : public PollWorker
{
public:
    // Work item used to terminate the poll loop
    struct Terminate{};

public:
    // The first fd is still the work item pipe, and we don't really do anything
    // special with it here.  The thread then hands in work items that come from
    // the queue though when the pipe is signaled.
    PollThreadWorker(std::function<void(Any)> userWorkHandler, int fd,
                     std::function<void()> handler)
        : _running{true}, _userWorkHandler{std::move(userWorkHandler)}
    {
        // Set up the EventLoop integration for this thread
        EventLoop::setThreadEventLoop(std::unique_ptr<EventLoop>{new PollThreadEventLoop(*this)});
        _workNotifier.activated = std::move(handler);
        _workNotifier.set(fd, PosixFdNotifier::WatchType::Read);
        assert(_userWorkHandler);  // Ensured by caller
    }

public:
    // Poll until handle() is called with a Terminate
    void run();

    // Handle a work item
    void handle(Any item);

private:
    bool _running;
    PosixFdNotifier _workNotifier;
    std::function<void(Any)> _userWorkHandler;
};

int PollWorker::addFd(int fd, int events)
{
    pollfd newFd{};
    newFd.fd = fd;
    newFd.events = events;
    // revents is always zero.  This is important to ensure that fds added
    // during pollFds() by a handler are never activated immediately,
    // regardless of where they are actually placed in _fds.

    // Look for a vacant entry in _fds to reuse.  This prevents _fds and
    // _handlers from growing without bound if a consumer cycles fds
    // periodically, while still simplifying pollFds() even if handlers remove
    // arbitrary fds.
    for(std::size_t i=0; i<_fds.size(); ++i)
    {
        if(_fds[i].fd == PosixFd::Invalid)
        {
            _fds[i] = std::move(newFd);
            // The tokens used by PollWorker are actually indices into _fds
            return static_cast<int>(i);
        }
    }

    // Otherwise, there are no vacant entries, add to the end.
    _fds.push_back(newFd);
    return static_cast<int>(_fds.size()-1);
}

void PollWorker::removeFd(int token)
{
    if(token < 0 || static_cast<std::size_t>(token) >= _fds.size())
    {
        KAPPS_CORE_WARNING() << "Attempted to remove token" << token
            << "that does not exist";
        return;
    }

    auto &fd = _fds[static_cast<std::size_t>(token)];
    if(fd.fd == PosixFd::Invalid)
    {
        KAPPS_CORE_WARNING() << "Attempted to remove token" << token
            << "that was not in use";
        return;
    }

    // Don't actually remove the entry from _fds.  We'll reuse the vacant entry
    // if another fd is added.  We use these indices as tokens, and we can't
    // invalidate them by erasing from the middle.  We could probably erase at
    // the end, but since this can occur during pollFds(), it is simpler if we
    // leave the vacant entries.
    fd.fd = PosixFd::Invalid;
    fd.events = 0;
}

void PollWorker::pollFds()
{
    // If there are no file descriptors, throw.  POSIX doesn't really say what
    // poll(2) would do in that case, but callers should never do this anyway -
    // either we'd sit here forever waiting on nothing, or we'd return
    // immediately and likely get stuck in a busy-wait loop.
    //
    // This never happens with PollThread, which always adds its work item pipe
    // as the first file descriptor.
    if(_fds.empty())
        throw std::runtime_error{"PollWorker::pollFds() requires at least one file descriptor"};

    int pollResult{-1};
    NO_EINTR(pollResult = ::poll(_fds.data(), _fds.size(), -1));

    // If the poll failed, trace it and do not process events - revents may
    // contain stale events from last time.
    //
    // BSD poll() can fail with EAGAIN, which just means we should try again,
    // but this is still traced in case it keeps failing (we'd be cranking the
    // CPU if that was to happen).
    //
    // Most other errors shouldn't really be retried, like EINAL, EFAULT,
    // ENOMEM, etc., but since all callers currently use a static set of file
    // descriptors (and no multiplexers, etc.), we never expect to see these.
    // We'll trace and loop if it did happen somehow, which may crank the CPU
    // but at least will not cause more serious problems.
    //
    // Finally, pollResult == 0 indicates a timeout, which we don't expect to
    // see here - but definitely don't process revents if it somehow occurred.
    if(pollResult <= 0)
    {
        KAPPS_CORE_WARNING() << "Poll failed:" << ErrnoTracer{};
        return;
    }

    // Process each file descriptor.  Note that this processing _can_ add more
    // file descriptors, which can invalidate iterators, so use an index.  The
    // vector has O(1) random access so this scales fine.
    for(std::size_t i=0; i<_fds.size(); ++i)
    {
        auto fd = _fds[i].fd;
        auto revents = _fds[i].revents;

        // Always trace POLLERR/POLLNVAL/POLLHUP.  Trace POLLERR and POLLNVAL
        // at warning, this is not expected - callers should not watch invalid fds
        // or close them while we're using them
        if(revents & POLLERR)
        {
            KAPPS_CORE_WARNING() << "Error polling file descriptor"
                << fd << "- got events" << revents;
        }
        if(revents & POLLNVAL)
        {
            KAPPS_CORE_WARNING() << "Invalid file descriptor" << fd
                << "- got events" << revents;
        }
        // POLLHUP may be expected - remote side hung up.  Log at info, it still
        // doesn't happen unreasonably often and usually indicates something is
        // shutting down, etc.
        if(revents & POLLHUP)
        {
            KAPPS_CORE_INFO() << "File descriptor" << fd
                << "was hung up - got events" << revents;
        }

        // If any events triggered, invoke activated().  The token is just the
        // index in _fds.
        if(revents)
            activated(static_cast<int>(i));
    }
}

PollThread::PollThread(std::function<void(Any)> workFunc)
{
    // Make the work-item-signaling pipe.  On Linux, an eventfd would be a tad
    // more efficient, but that's not available on macOS and probably isn't
    // worth the abstraction
    auto pipe = createPipe();
    // Keep the write end on this thread
    _itemSignalPipe = std::move(pipe.writeEnd);

    // Start up the worker thread.
    // - We can safely capture this because the destructor joins the thread
    // - Move the read end of the pipe to this thread
    // - Pass workFunc all the way through to the PollThreadWorker, the outer
    //   lamba is mutable so we can move it again
    _workThread = std::thread{[this, itemPipe = std::move(pipe.readEnd), workFunc = std::move(workFunc)]() mutable
    {
        PollThreadWorker worker{std::move(workFunc), itemPipe.get(),
            [this, &itemPipe, &worker]()
            {
                // Drain the signal pipe
                itemPipe.discardAll();
                std::lock_guard<std::mutex> lock{_itemsMutex};
                while(!_items.empty())
                {
                    // Take the front item
                    Any item{std::move(_items.front())};
                    _items.pop();
                    worker.handle(std::move(item));
                }
            }};
        worker.run();
    }};
}

PollThread::~PollThread()
{
    // Just tell the thread to exit, then wait for it
    enqueue(PollThreadWorker::Terminate{});
    assert(_workThread.joinable()); // Class invariant
    _workThread.join();
}

void PollThread::enqueue(Any item)
{
    {
        std::lock_guard<std::mutex> lock{_itemsMutex};
        _items.push(std::move(item));
    }
    // Write to the signal pipe to signal the worker thread
    unsigned char data{};
    ::write(_itemSignalPipe.get(), &data, sizeof(data));
}

void PollThread::queueInvoke(std::function<void()> func)
{
    enqueue(WorkFunc<>{std::move(func)});
}

void PollThread::syncInvoke(std::function<void()> func)
{
    SyncWorkFunc syncWork;
    enqueue(syncWork.work<>(std::move(func)));
    syncWork.wait();
}

void PollThreadWorker::run()
{
    while(_running)
        pollFds();
}

void PollThreadWorker::handle(Any item)
{
    if(item.containsType<Terminate>())
    {
        // We're done, terminate the poll loop
        _running = false;
        return;
    }
    // Invoke WorkFunc<void> values automatically
    else if(item.containsType<WorkFunc<>>())
        item.handle<WorkFunc<>>([](WorkFunc<> &func){func.invoke();});
    else
    {
        // Otherwise, pass it to the user's work func
        assert(_userWorkHandler);  // Class invariant
        _userWorkHandler(std::move(item));
    }
}

}}
