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
#include <kapps_core/src/eventloop.h>
#include <kapps_core/src/coresignal.h>

namespace kapps { namespace core {

// Notify when a POSIX file descriptor has events ready.  Each PosixFdNotifier
// manages a single file descriptor and watch type (use two notifiers to watch
// both read and write)
//
// When the indicated events are received, the activated signal is emitted.
class KAPPS_CORE_EXPORT PosixFdNotifier
{
private:
    // EventLoop::fdWatchTriggered() can call triggered()
    friend class EventLoop;

    // Called by EventLoop to indicate that a watch has triggered.
    static void watchTriggered(EventLoop::TokenT token);

public:
    using WatchType = EventLoop::WatchType;

public:
    // Initially, PosixFdNotifier is inactive.  Use set() to activate it.  If no
    // EventLoop is set on this thread, PosixFdNotifers cannot be created at all,
    // so this throws.
    PosixFdNotifier();
    ~PosixFdNotifier();
    PosixFdNotifier(PosixFdNotifier &&other);
    PosixFdNotifier &operator=(PosixFdNotifier &&other);

private:
    // We could provide copy operations by creating a new watch, but there's
    // hardly any reason to do this.  Usually the file descriptor is owned by
    // the caller, and it can't be copied anyway.
    PosixFdNotifier(PosixFdNotifier &) = delete;
    PosixFdNotifier &operator=(PosixFdNotifier &) = delete;

private:
    void triggered();

public:
    // Check whether the notifier is active
    bool active() const {return _token != EventLoop::InvalidToken;}
    explicit operator bool() const {return active();}
    bool operator!() const {return !active();}

    // Watch a file descriptor.  If a watch is already active, it's canceled.
    //
    // While EventLoop::watchFd() should avoid failing as much as possible, if
    // it does fail, set() simply does not activate (it does not throw).
    //
    // The file descriptor must be valid, and it is the caller's responsibility
    // to cancel the watch if it could be closed (do not rely on POLLNVAL for
    // this, as the file descriptor could be reused once it has become invalid).
    // Usually this happens automatically if the file descriptor is held in a
    // PosixFd owned by the same object as this PosixFdNotifier.
    void set(int fd, WatchType type);

    // Cancel the watch, if active.  No effect if not active.
    void cancel();

public:
    // Signal triggerd when the watch triggers with events matching the
    // requested events.
    Signal<> activated;

private:
    // The current watch token, if active.  Note that 0 may be a valid token;
    // EventLoop::InvalidToken indicates no watch.
    EventLoop::TokenT _token;
    // The monitored file descriptor, mainly just for tracing currently.
    int _fd;
};

}}
