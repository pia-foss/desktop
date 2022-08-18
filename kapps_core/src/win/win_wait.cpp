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

#include "win_wait.h"
#include "win_error.h"
#include <kapps_core/src/logger.h>
#include <cassert>

#pragma comment(lib, "Kernel32.lib")

namespace kapps { namespace core {

void CALLBACK WinSingleWait::waitCallback(void *pParam, BOOLEAN timedOut)
{
    assert(!timedOut);  // Timeout not used currently
    assert(pParam); // Class invariant; always specified

    WinSingleWait *pThis = reinterpret_cast<WinSingleWait*>(pParam);
    pThis->_callback(pThis->_waitOnObject);
}

WinSingleWait::WinSingleWait(HANDLE waitOnObject,
                             std::function<void(HANDLE)> callback, bool once)
    : _waitOnObject{waitOnObject}, _callback{std::move(callback)}
{
    assert(_callback);  // Ensured by caller

    DWORD flags{WT_EXECUTEDEFAULT};
    if(once)
        flags |= WT_EXECUTEONLYONCE;

    if(!RegisterWaitForSingleObject(_wait.receive(), waitOnObject,
        &WinSingleWait::waitCallback, this, INFINITE, flags))
    {
        KAPPS_CORE_WARNING() << "Unable to register wait on handle:"
            << WinErrTracer{::GetLastError()};
    }
}

}}
