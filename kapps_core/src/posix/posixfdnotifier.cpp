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

#include "posixfdnotifier.h"
#include "../logger.h"
#include "posix_objects.h"
#include <unordered_map>

namespace kapps { namespace core {

namespace
{
    // Map of active watch tokens to PosixFdNotifier objects on this thread,
    // used to find the object when it activates.
    thread_local std::unordered_map<EventLoop::TokenT, PosixFdNotifier*> activeNotifiers;
}

void PosixFdNotifier::watchTriggered(EventLoop::TokenT token)
{
    auto itActiveNotifier = activeNotifiers.find(token);
    // The event loop shouldn't invoke a timer that was not set or canceled -
    // this is a serious error and could cause misfires if the ID had been
    // reused.
    if(itActiveNotifier == activeNotifiers.end())
    {
        KAPPS_CORE_WARNING() << "Notifier" << token << "was invoked but was not active";
        throw std::runtime_error{"Notifier invoked that was not active"};
    }

    assert(itActiveNotifier->second);   // Class invariant
    itActiveNotifier->second->triggered();
}

PosixFdNotifier::PosixFdNotifier()
    : _token{EventLoop::InvalidToken}, _fd{PosixFd::Invalid}
{}

PosixFdNotifier::~PosixFdNotifier()
{
    cancel();
}

PosixFdNotifier::PosixFdNotifier(PosixFdNotifier &&other)
    : PosixFdNotifier{}
{
    *this = std::move(other);
}

PosixFdNotifier &PosixFdNotifier::operator=(PosixFdNotifier &&other)
{
    std::swap(_token, other._token);
    std::swap(_fd, other._fd);

    if(active())
        activeNotifiers[_token] = this;
    if(other.active())
        activeNotifiers[other._token] = &other;
    return *this;
}

void PosixFdNotifier::triggered()
{
    // If any relevant events were received, invoke the activated signal.
    // Note that _events is still valid at this point even if we just
    // canceled; cancel() leaves it alone for this reason.
    activated();
}

void PosixFdNotifier::set(int fd, WatchType type)
{
    cancel();

    _token = EventLoop::getThreadEventLoop().watchFd(fd, type);
    // It's possible for the factory to fail, though it should avoid this as
    // much as possible.
    if(!active())
    {
        KAPPS_CORE_WARNING() << "Event loop failed to watch file desccriptor"
            << fd;;
        return;
    }

    PosixFdNotifier *&pTokenNotifier = activeNotifiers[_token];
    // Should be a new entry - if it was already set, the event loop returned
    // a duplicte ID, which is a serious error.
    if(pTokenNotifier)
    {
        KAPPS_CORE_WARNING() << "Event loop returned duplicate fd watch token"
            << _token << "for fd" << fd;

        // There's not much we can do to recover from this, but drop the token
        // since another notifier already has it to limit the damage.
        _token = EventLoop::InvalidToken;
        return;
    }

    pTokenNotifier = this;
    _fd = fd;
}

void PosixFdNotifier::cancel()
{
    if(active())
    {
        EventLoop::getThreadEventLoop().cancelFdWatch(_token);
        activeNotifiers.erase(_token);
        _token = EventLoop::InvalidToken;
        _fd = PosixFd::Invalid;
    }
}

}}
