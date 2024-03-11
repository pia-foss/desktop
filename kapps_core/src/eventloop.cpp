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

#include "eventloop.h"
#include "timer.h"
#include <memory>
#include <stdexcept>

#if defined(KAPPS_CORE_OS_POSIX)
#include "posix/posixfdnotifier.h"
#endif

namespace kapps { namespace core {

namespace
{
    thread_local std::unique_ptr<EventLoop> pThreadEventLoop;
}

void EventLoop::setThreadEventLoop(std::unique_ptr<EventLoop> pEventLoop)
{
    // Changing the thread's event loop wouldn't make sense; this would likely
    // invalidate all timer/watch tokens.
    if(pThreadEventLoop)
        throw std::runtime_error{"Can't change thread event loop implementation once set"};
    if(!pEventLoop)
        throw std::runtime_error{"Invalid event loop implementation"};
    pThreadEventLoop = std::move(pEventLoop);
}

EventLoop &EventLoop::getThreadEventLoop()
{
    if(!pThreadEventLoop)
        throw std::runtime_error{"Thread does not have an event loop implementation"};
    return *pThreadEventLoop;
}

void EventLoop::timerElapsed(TokenT token)
{
    Timer::timerElapsed(token);
}

#if defined(KAPPS_CORE_OS_POSIX)
void EventLoop::fdWatchTriggered(TokenT token)
{
    PosixFdNotifier::watchTriggered(token);
}
#endif

}}
