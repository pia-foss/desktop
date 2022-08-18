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

#pragma once
#include <kapps_core/core.h>
#include <memory>
#include <chrono>

namespace kapps { namespace core {

// EventLoop provides integrations between the kapps libraries and the thread's
// event loop.  The kapps libraries do not mandate a particular event loop
// structure, so these primitives allow us to tie into whatever event loop is
// being used on this thread.
//
// **********
// * Timers *
// **********
//
// EventLoop provides APIs to create timers.  Usually there is a platform API
// suitable for use as a timer source, most common timer APIs conform to the
// requirements below.
//
// Specifically, TimerFactory timers:
// - Use intervals of monotonic time - not wall-clock time
// - Should not include time spent in system suspend/hibernation states
// - Can fire slightly early if the system optimizes timers to minimize wakeups
//   (Some system APIs do this; Qt timers may fire up to 5% early by default)
//
// Both single-shot and recurring timers can be created.  Most platforms have
// specific support for both, and the "default" behavior varies widely (e.g. on
// some platforms a "single-shot" timer is really a recurring timer that's
// automatically canceled, but on other systems a "recurring" timer is really a
// single-shot timer that's automatically rescheduled.  Picking one style would
// be a poor fit for opposite-style systems.)
//
// kapps::core::Timer is implemented with this API.
//
// ***************************
// * File descriptor watches *
// ***************************
//
// On POSIX, EventLoop provides APIs to monitor file descriptors, such as with
// poll(2).  Callers provide their events of interest when creating the watch.
//
// kapps::core::PosixFdNotifier is implemented with this API.
//
// ******************
// * Windows Events *
// ******************
//
// TODO - On Windows, there will be APIs to monitor Events, or in general any
// HANDLE to an object that can be signaled (those listed with
// WaitForMultipleObjectsEx(), etc.)
//
// *******************
// * Implementations *
// *******************
//
// Consumers of kapps libraries should provide an implementation of this
// interface.  Currently, this is mainly only needed on the "main thread" -
// precisely, the one that calls methods of kapps::net::Firewall.  In the future,
// this may be needed on other threads too.
//
// Usually, all elements of the API should be implemented.  In some rare cases,
// intenral auxiliary threads might partially implement this interface when it
// is known that some parts are not needed on that thread.
//
// On POSIX, kapps::core::PollThread auxiliary threads provide an event loop
// and a specific integration.
class KAPPS_CORE_EXPORT EventLoop
{
public:
    // A created timer, or monitored file descriptor, is identified by a token -
    // an integer referring to it.  The token is used to indicate that the timer
    // or file descriptor has been triggered, and it's used to cancel the timer
    // or file descriptor later.
    //
    // Values <0 are not valid tokens, this indicates failure from setTimer(),
    // etc.  (0 can be a valid token, some systems use this.)
    //
    // Both timers and fd watches use the same type of token, but they are
    // different namespaces - the same numeric token could refer to a valid
    // timer and fd watch simultaneously.  (It's up to the implementation to
    // choose whether it actually does this.)
    using TokenT = int;

    enum : TokenT
    {
        InvalidToken = -1
    };

    enum class WatchType
    {
        Read,
        Write,
    };

public:
    // Set the EventLoop implementation for this thread.  The implementation
    // becomes owned by the thread and cannot be changed after it is set.
    static void setThreadEventLoop(std::unique_ptr<EventLoop> pEventLoop);

    // Get the EventLoop implementation for this thread - used in kapps libraries.
    // Throws if it hasn't been set yet.  Otherwise, any valid object returned
    // remains valid until the thread exits.
    static EventLoop &getThreadEventLoop();

public:
    virtual ~EventLoop() = default;

protected:
    // Invoke timerElapsed() from your event loop when a timer configured with
    // EventLoop has elapsed.
    //
    // Never invoke this from EventLoop::setTimer(); timers with interval 0
    // should be invoked upon returning to the event loop.
    void timerElapsed(TokenT token);

#if defined(KAPPS_CORE_OS_POSIX)
    // On UNIX-like OSes, indicate that a file descriptor watch has fired.
    // Provide the revents.  Unlike timers, a file descriptor watch never
    // automatically cancels (even if it receives POLLHUP/POLLERR/POLLNVAL), the
    // receiver should cancel it in that case.
    void fdWatchTriggered(TokenT token);
#endif

public:
    // Create a timer.  After 'interval' elapses, call timerElapsed(token),
    // where token is the returned timer token.  'interval' may be 0 for
    // single-shot timers, in which case schedule the timer to fire as soon as
    // possible when returning to the event loop (do not invoke the timer
    // reentrantly from setTimer()).
    //
    // If single is true, this is a single-shot timer.  Cancel the timer before
    // invoking elapsed().  Most platform APIs provide this functionality, so
    // hook this up directly to the platform implementation.
    //
    // If single is false, this is a recurring timer.  Invoke elapsed(receiver)
    // every 'interval' until the timer is canceled (which could occur during
    // an invocation of elapsed().)
    //
    // If recurring timers fall so far behind that the interval elapses again
    // while a call to elapsed() for that timer is still pending, the additional
    // triggers can be silently discarded.
    //
    // While this can return InvalidToken to indicate failure to schedule the
    // timer, this should be avoided as much as possible, since it's not always
    // possible to recover from this without significant degradation.
    virtual TokenT setTimer(std::chrono::milliseconds interval, bool single) = 0;

    // Cancel a timer previously created by setTimer().  The token value can
    // then be reused by a subsequent call to setTimer().
    //
    // Triggers for the timer being canceled _must not_ be invoked after this
    // (since the token value could be reused by a subsequent timer).  For
    // example, if you maintain a list of triggered timer invocations that are
    // pending, you must remove any invocations for this timer from that list as
    // well as canceling future triggers.
    virtual void cancelTimer(TokenT token) = 0;

#if defined(KAPPS_CORE_OS_POSIX)
    // On UNIX-like OSes, monitor a file descriptor as if with poll(2), etc.
    //
    // Either "read" or "write" events can be requested
    //
    // A "read" watch must activate for POLLIN or POLLHUP - hangups must be
    // detected as this is a significant input event (this is relevant to
    // Process in particular).  The specific event (POLLIN, POLLHUP, or both)
    // is not indicated however, as some event loops don't provide this.
    //
    // A "write" watch must activate for POLLOUT or POLLHUP.
    //
    // POLLERR and POLLNVAL are not required in any case.  The implementation
    // also must not watch POLLIN/POLLOUT when _not_ requested - this may
    // trigger constantly since no action is being taken.
    //
    // The caller continues to own the file descriptor and is responsible for
    // canceling the watch before closing it.
    //
    // The event loop does not have to support multiple watches of the same
    // type for the same file descriptor.  If this is requested, reject the
    // subsequent watches by returning InvalidToken if necessary.
    virtual TokenT watchFd(int fd, WatchType type) = 0;

    // Cancel a file descriptor watch.
    virtual void cancelFdWatch(TokenT token) = 0;
#endif
};

}}
