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
#include "winapi.h"
#include "win_handle.h"
#include <functional>

namespace kapps { namespace core {

// Wait on a single handle (using RegisterWaitForSingleObject()).
//
// IMPORTANT: The callback will be executed on a worker thread from the system
// thread pool.  DO NOT destroy WinSingleWait during the callback.  (This
// unregisters the callback, which can't occur during the callback.)
// 
// In general, applications should usually queue the notification to another
// thread and process it there; consider using WorkThread for this.
class KAPPS_CORE_EXPORT WinSingleWait
{
private:
    struct WinCloseWait
    {
        void operator()(HANDLE wait){::UnregisterWait(wait);}
    };
    using WinWaitHandle = WinGenericHandle<HANDLE, WinCloseWait>;
    
    static void CALLBACK waitCallback(void *pParam, BOOLEAN timedOut);
    
private:
    // Not copiable or movable - although the WinWaitHandle and function are
    // both movable, the wait handle contains 'this' as a context pointer and
    // can't be updated.
    WinSingleWait(const WinSingleWait &) = delete;
    WinSingleWait &operator=(const WinSingleWait &) = delete;

public:
    // Wait on a single object, and invoke callback on the worker thread when it
    // is signaled.
    // The handle cannot be closed before WinSingleWait is destroyed.
    // 'once' indicates that the callback is only invoked the first time the
    // object becomes signaled.  (Otherwise it is invoked again if the object is
    // signaled again.)  The wait must still be unregistered, so the destruction
    // caveat still applies.
    WinSingleWait(HANDLE waitOnObject, std::function<void(HANDLE)> callback,
                  bool once);

private:
    WinWaitHandle _wait;
    HANDLE _waitOnObject;
    std::function<void(HANDLE)> _callback;
};

}}
