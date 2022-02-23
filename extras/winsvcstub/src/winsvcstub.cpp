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

#include "common.h"
#line SOURCE_FILE("winsvcstub.cpp")

#include <Windows.h>
#include "win_util.h"

#pragma comment(lib, "Advapi32.lib")    // Service functions

void WINAPI svcMain(DWORD dwNumServicesArgs, LPWSTR *lpServiceArgVectors);
DWORD WINAPI svcCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);

WinHandle exitEvent;

int main(int argc, char **argv)
{
    SERVICE_TABLE_ENTRYW svcTable[] =
    {
        { L"Dnscache", &svcMain },
        { nullptr, nullptr }
    };

    exitEvent = WinHandle{::CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    if(!exitEvent)
        return 1;   // Can't start service, we wouldn't be able to tell it to exit

    return !::StartServiceCtrlDispatcherW(svcTable);
}

void WINAPI svcMain(DWORD dwNumServicesArgs, LPWSTR *lpServiceArgVectors)
{
    SERVICE_STATUS_HANDLE statusHandle;
    statusHandle = ::RegisterServiceCtrlHandlerExW(L"Dnscache", &svcCtrlHandler,
                                                   nullptr);

    SERVICE_STATUS serviceStatus{
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_RUNNING,
        SERVICE_ACCEPT_STOP,
        NO_ERROR,
        0,
        0,
        0
    };
    // Result ignored, nothing we can do if it fails
    ::SetServiceStatus(statusHandle, &serviceStatus);

    // Wait for the service to be stopped
    WaitForSingleObject(exitEvent.get(), INFINITE);

    // Report that we've stopped
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceStatus.dwControlsAccepted = 0;
    ::SetServiceStatus(statusHandle, &serviceStatus);

    // That's it, we're done
}

DWORD WINAPI svcCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    // We can get INTERROGATE at any time, but we just have to return NO_ERROR,
    // the SCM already knows the state of the service.
    if(dwControl == SERVICE_CONTROL_INTERROGATE)
        return NO_ERROR;

    // Other than INTERROGATE, we only implement the STOP control
    if(dwControl != SERVICE_CONTROL_STOP)
        return ERROR_CALL_NOT_IMPLEMENTED;

    // Stop the service
    return ::SetEvent(exitEvent.get()) ? NO_ERROR : ERROR_FAIL_SHUTDOWN;
}
