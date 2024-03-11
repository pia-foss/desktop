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
#include "eventloop.h"
#include "coresignal.h"

namespace kapps { namespace core {

// A Timer implemented using EventLoop.  Each Timer manages a single timer
// (use multiple Timers if needed).  Timer invokes its timeout signal when
// elapsed.
class KAPPS_CORE_EXPORT Timer
{
private:
    // EventLoop::timerElapsed() can call elapsed()
    friend class EventLoop;

    // Called by EventLoop to indicate that a timer elapsed
    static void timerElapsed(EventLoop::TokenT token);

public:
    // Initially, Timer is inactive.  Set a timer with setTimer().
    // If no TimerFactory is set, Timer objects cannot be created at all, so
    // this throws.
    Timer();
    ~Timer();
    Timer(Timer &&other);
    Timer &operator=(Timer &&other);

private:
    // Not copiable; we'd need an API to duplicate a timer.  Scheduling a new
    // timer with the same interval would not be correct since some time has
    // already passed.
    Timer(Timer &) = delete;
    Timer &operator=(Timer &) = delete;

public:
    // Check whether the timer is active
    bool active() const {return _token != EventLoop::InvalidToken;}
    explicit operator bool() const {return active();}
    bool operator!() const {return !active();}

    // Set a timer.  If a timer is already active, it's canceled.
    //
    // While EventLoop::setTimer() should avoid failing as much as possible,
    // if it does fail, set() simply does not activate (it does not throw).
    // See the implementation for details.
    void set(std::chrono::milliseconds interval, bool single);

    // Cancel the timer, if active.  No effect if not active.
    void cancel();

public:
    // Signal triggered when the timer elapses
    Signal<> elapsed;

private:
    // The current timer token, if active.  EventLoop::InvalidToken indicates
    // the timer is not active.  (Note that 0 may be a valid token.)
    EventLoop::TokenT _token;
    // When active, whether this is a single-shot timer - needed so we know to
    // clear _token automatically when elapsed.
    bool _single;
};

}}
