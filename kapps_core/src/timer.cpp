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

#include "timer.h"
#include "logger.h"
#include <unordered_map>

namespace kapps { namespace core {

namespace
{
    // Map of active timer tokens to timers on this thread; used to find a timer
    // when it elapses
    thread_local std::unordered_map<EventLoop::TokenT, Timer*> activeTimerTokens;
}

void Timer::timerElapsed(EventLoop::TokenT token)
{
    auto itActiveTimer = activeTimerTokens.find(token);
    // This should not happen - the product invoked a timer that is not set or
    // was canceled.  This is a serious error as it could cause timer misfires
    // if a timer ID had already been reused.
    if(itActiveTimer == activeTimerTokens.end())
    {
        KAPPS_CORE_WARNING() << "Timer" << token << "was invoked but was not active";
        throw std::runtime_error{"Timer invoked that was not active"};
    }

    assert(itActiveTimer->second);  // Class invariant

    // If it's a single-shot timer, the timer was already canceled, clear _token
    if(itActiveTimer->second->_single)
        itActiveTimer->second->_token = EventLoop::InvalidToken;
    itActiveTimer->second->elapsed();
}

Timer::Timer()
    : _token{EventLoop::InvalidToken}, _single{false}
{
    // If the event loop integration hasn't been set yet, we can't create any
    // timers.  This has to be done in intiailization
    EventLoop::getThreadEventLoop();
}

Timer::~Timer()
{
    cancel();
}

Timer::Timer(Timer &&other)
    : Timer{}
{
    *this = std::move(other);
}

Timer &Timer::operator=(Timer &&other)
{
    std::swap(_token, other._token);
    std::swap(_single, other._single);

    // Update references in activeTimerTokens
    if(active())
        activeTimerTokens[_token] = this;
    if(other.active())
        activeTimerTokens[other._token] = &other;
    return *this;
}

void Timer::set(std::chrono::milliseconds interval, bool single)
{
    cancel();
    _single = single;

    _token = EventLoop::getThreadEventLoop().setTimer(interval, single);

    // It's possible for the factory to fail, though it should avoid this as
    // much as possible
    if(!active())
    {
        KAPPS_CORE_WARNING() << "Event loop failed to set timer with interval"
            << traceMsec(interval) << "and single=" << single;

        // Just eat the error, don't throw.  The caller could check active()
        // after set() if it really wants to, but often there is nothing more
        // we can do to gracefully degrade.
        return;
    }

    Timer *&pTokenTimer = activeTimerTokens[_token];
    // This should be a new entry - if it was already set to something, the
    // timer factory returned a duplicate ID
    if(pTokenTimer)
    {
        KAPPS_CORE_WARNING() << "Event loop returned duplicate timer token"
            << _token << "for interval" << traceMsec(interval) << "and single="
            << single;

        // There's not much we can do to recover from this, but drop the token
        // since another timer already has it to limit the damage.
        _token = EventLoop::InvalidToken;
        return;
    }

    pTokenTimer = this;
}

void Timer::cancel()
{
    if(active())
    {
        EventLoop::getThreadEventLoop().cancelTimer(_token);
        activeTimerTokens.erase(_token);
        _token = EventLoop::InvalidToken;
    }
}

}}
