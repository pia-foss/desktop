// Copyright (c) 2020 Private Internet Access, Inc.
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

#include "process.h"
#include "brand.h"

#include <tlhelp32.h>

bool isProcessRunning(utf16ptr executable)
{
    bool isRunning = false;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32 process {};
    process.dwSize = sizeof(process);
    for (BOOL valid = Process32First(snapshot, &process); valid; valid = Process32Next(snapshot, &process))
    {
        if (0 == lstrcmpiW(process.szExeFile, executable))
        {
            isRunning = true;
            break;
        }
    }

    CloseHandle(snapshot);

    return isRunning;
}

void KillExistingClientsTask::execute()
{
    LOG("Killing existing clients");
    _listener->setCaption(IDS_CAPTION_SHUTTINGDOWNCLIENT);

    UINT msg = RegisterWindowMessageW(L"WM_PIA_EXIT_CLIENT");
    if (msg == 0)
        LOG("Failed to register exit message (%d)", GetLastError());

retry:
    bool success = true;

    if (msg)
        PostMessage(HWND_BROADCAST, msg, 0, 0);

    // Get a snapshot of all running processes on the machine
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return; // Something is wrong, but the error message would be confusing so try to continue silently

    std::vector<HANDLE> waitProcesses;

    // Iterate over the snapshot
    PROCESSENTRY32 process;
    process.dwSize = sizeof(process);
    for (BOOL valid = Process32First(snapshot, &process); valid; valid = Process32Next(snapshot, &process))
    {
        // Check the executable path of the process
        if (0 != lstrcmpi(process.szExeFile, _T(BRAND_CODE "-client.exe")) &&
                0 != lstrcmpi(process.szExeFile, _T(BRAND_CODE "-client-portable.exe")))
            continue;

        HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, process.th32ProcessID);
        if (processHandle == INVALID_HANDLE_VALUE)
            continue;

        // TODO: Check full executable path against clientPath

        // Find any window belonging to this process and send WM_CLOSE to it
        struct Query { DWORD pid; bool found; } query = { process.th32ProcessID, false };
        EnumWindows([](HWND hwnd, LPARAM lParam) {
            auto query = reinterpret_cast<Query*>(lParam);
            DWORD windowProcessID;
            GetWindowThreadProcessId(hwnd, &windowProcessID);
            if (windowProcessID == query->pid)
            {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                query->found = true;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&query));

        // If no windows were found, send a terminate signal
        if (!query.found)
            TerminateProcess(processHandle, 0);
        waitProcesses.push_back(processHandle);
    }

    // Done iterating
    CloseHandle(snapshot);

    // Wait for any processes that we tried to terminate
    if (!waitProcesses.empty())
    {
        DWORD waitResult = WaitForMultipleObjects((DWORD)waitProcesses.size(), waitProcesses.data(), TRUE, 2000);
        if (!(waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + waitProcesses.size()))
        {
            // Send a hard terminate signal to the processes and wait again
            for (HANDLE process : waitProcesses)
                TerminateProcess(process, 0);
            waitResult = WaitForMultipleObjects((DWORD)waitProcesses.size(), waitProcesses.data(), TRUE, 2000);
            if (!(waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + waitProcesses.size()))
            {
                // Still processes alive despite our best efforts; signal failure
                success = false;
            }
        }
        for (HANDLE process : waitProcesses)
            CloseHandle(process);
    }

    // Throw on failure
    if (!success)
    {
        if (Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_STILLCLIENTSRUNNING))
            goto retry;
    }
}
