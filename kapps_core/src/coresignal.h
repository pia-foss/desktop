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
#include <functional>
#include <mutex>

namespace kapps { namespace core {

// Signal provides a mechanism for an object to emit a synchronous signal
// indicating an event.
//
// It's mainly a std::function<> wrapper that allows the function to be empty -
// calls to an empty signal are simply ignored.  Signals do not return a value;
// the function return type is always void.
//
// Typically, declare a signal as a public member of a class, such as:
//
// class EventMonitor
// {
// public:
//     kapps::core::Signal<void> event;
// };
//
// A consumer can then connect to the signal:
//
// EventMonitor createMonitor{...};
// createMonitor.event = []{std::cout << "Create occured";};
//
// Callback can capture 'this', but Signal has no way to disconnect
// automatically if 'this' is destroyed.  Typically this should only be done for
// owned objects, and destructors of objects with signals should not emit
// signals.
template<class... Args>
class Signal
{
public:
    // The Signal is initially empty.  It can be copied and moved - copying it
    // results in two Signals referring to the same callback, and moving it
    // moves the callback.
    
public:
    Signal &operator=(std::function<void(Args...)> callback)
    {
        _callback = std::move(callback);
        return *this;
    }
    
    template<class... CallArgs>
    void operator()(CallArgs&&... args) const
    {
        if(_callback)
            _callback(std::forward<CallArgs>(args)...);
    }

private:
    std::function<void(Args...)> _callback;
};

// ThreadSignal is a Signal that's thread-safe - it can be assigned and invoked
// from any thread.  Note that the connected function is still invoked on
// whatever thread the signal was invoked on.
template<class... Args>
class ThreadSignal
{
public:
    ThreadSignal &operator=(std::function<void(Args...)> callback)
    {
        std::lock_guard<std::mutex> lock{_mutex};
        _callback = std::move(callback);
        return *this;
    }
    
    template<class... CallArgs>
    void operator()(CallArgs&&... args) const
    {
        // Grab the callback while locked, but then invoke the copy outside the
        // lock.  This allows two mutually-connected objects to signal each
        // other without creating a potential deadlock, and it also allows
        // ThreadSignal to be destroyed during the callback.
        std::function<void(Args...)> currentCallback;
        {
            std::lock_guard<std::mutex> lock{_mutex};
            currentCallback = _callback;
        }
        if(currentCallback)
            currentCallback(std::forward<CallArgs>(args)...);
    }

private:
    mutable std::mutex _mutex;
    std::function<void(Args...)> _callback;
};

}}
