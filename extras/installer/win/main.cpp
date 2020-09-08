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

/*
Installer architecture:

The Windows installer is written directly against the Win32 API in order to
achieve a zero-dependency executable (statically linked C/C++ runtime) when
executed on Windows 7 and above.

If errors are encountered or the user aborts the installation, the installer
implements a rollback mechanism to undo (most) changes to the system.

The installer uses a two-thread approach, the main thread creating a window
and a message loop (even in silent mode), while launching a separate worker
thread for the actual installation tasks. The main program flow thus lies
in the worker thread, while the main thread keeps the installer responsive.
Mutexes are used to synchronize important state.

Program flow:

             Main thread                         Worker thread
                  |                                    .
           (create window)                             .
                  |                                    .
           (create thread)---------------------------->*
                  |                                    |
         (enter messageloop)                  (install prechecks)
                  |                                    |
                  |<--------(ready to install)---------+
                  |                                    |
           [ click start ]                             |
                  |                                    |
                  +--------(start installation)------->+
                  |                                    |
                  |                        +---------->|
                  |                        |           |
           [ click abort ] . . . .         |    (perform tasks)
                  |              .         |           |
                  |              .         |     <check error>-----+
                  |              .         |           |           |
                  |              . . . . . | . . <check abort>-----+
                  |                        |           |           |
                  |                        +--------<done?>        |
                  |                                    |           |
                  |<------------(finished)-------------+           |
                  |                                    |           |
           [ click close ]                       (exit thread)     |
                  |                                    |           |
         (leave messageloop)                           |           |
                  |                                    |           |
            (join thread)------------------------------*           |
                  |                                    .           |
             (start app)                               .           |
                  |                                    .           |
                  *                                    .           |
                  .                                    .           |
---------------------------< error / rollback >---------------------------
                  |                                    |           |
                  |                                    |<----------+
                  |                                    |
                  |<------------(aborting)-------------+
                  |                                    |
                  |                           (roll back changes)
                  |                                    |
                  |<------------(finished)-------------+
                  |                                    |
           [ click close ]                       (exit thread)
                  |                                    |
         (leave messageloop)                           |
                  |                                    |
            (join thread)------------------------------*
                  |
                  *

Some actions, like starting the installation or acknowleding the result, are
skipped in silent or passive mode.

*/

#include "common.h"

#include <shlobj_core.h>
#include <shlwapi.h>
#include <userenv.h>

#include "resource.h"

#include "util.h"
#include "tasks/payload.h"
#include "tasks.h"
#include "installer.h"
#include "version.h"

int main();

extern "C" int CALLBACK _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
{
    // The very first thing we do is override the DLL search order for subsequent
    // LoadLibrary calls to only load system DLLs (and specifically not to check
    // the current executable directory). This provides a way to guard against
    // DLL overriding by delay-loading any DLLs outside the system's core set of
    // "known DLLs" with fixed locations (see winstaller.item.qbs).
    //
    // This function was provided in an update on Windows 7, so skip the call if
    // it isn't available, out-of-date 7 boxes don't support this hardening
    // measure.
    HMODULE kernel32 = ::LoadLibraryW(L"kernel32.dll");
    using SDDDFunc = BOOL (WINAPI *)(DWORD);
    SDDDFunc pSetDefaultDllDirectories = nullptr;

    if(kernel32)
    {
        pSetDefaultDllDirectories = reinterpret_cast<SDDDFunc>(::GetProcAddress(kernel32, "SetDefaultDllDirectories"));

        if(pSetDefaultDllDirectories)
            pSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);

        ::FreeLibrary(kernel32);
    }

    return main();
}

// Manually link against required system libraries
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "userenv.lib")


#include <utility>
#include <string>
#include <sstream>
#include <fstream>
#include <stack>
#include <list>
#include <vector>
#include <functional>
#include <exception>
#include <atomic>

// Configuration / command-line arguments
bool g_silent = false;          // /SILENT - run (un)installer without a GUI
bool g_passive = true;          // /PASSIVE - run installer without confirmations

// Main objects
HINSTANCE g_instance = NULL;

std::wstring g_executablePath;
std::wstring g_installPath;
std::wstring g_userTempPath;
std::wstring g_systemTempPath;
std::wstring g_startMenuPath;
std::wstring g_clientPath;
std::wstring g_servicePath;
std::wstring g_wgServicePath;
std::wstring g_clientDataPath;
std::wstring g_daemonDataPath;
std::wstring g_oldDaemonDataPath;

static void adjustProcessTokenPrivileges(std::initializer_list<std::pair<LPCTSTR, DWORD>> privileges)
{
    HANDLE token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
    {
        TOKEN_PRIVILEGES* p = (TOKEN_PRIVILEGES*)malloc(sizeof(TOKEN_PRIVILEGES) + (privileges.size() - 1) * sizeof(LUID_AND_ATTRIBUTES));
        p->PrivilegeCount = 0;

        for (const auto& pair : privileges)
        {
            LUID luid;
            if (LookupPrivilegeValue(NULL, pair.first, &luid))
            {
                p->Privileges[p->PrivilegeCount].Luid = luid;
                p->Privileges[p->PrivilegeCount].Attributes = pair.second;
                p->PrivilegeCount++;
            }
        }
        AdjustTokenPrivileges(token, FALSE, p, 0, NULL, NULL);
        CloseHandle(token);

        free(p);
    }
}

// Determine the path/filename of the (un)installer executable
static std::wstring getExecutablePath()
{
    std::wstring path(MAX_PATH, 0);
    path.resize(GetModuleFileName((HMODULE)g_instance, &path[0], path.size()));
    return path;
}

// Determine the system temp directory (e.g. C:\Windows\Temp)
static std::wstring getSystemTempPath()
{
    std::wstring path;
    const wchar_t* env;
    if (CreateEnvironmentBlock((LPVOID*)&env, NULL, FALSE))
    {
        for (const wchar_t* p = env; *p; p += wcslen(p) + 1)
        {
            if (!wcsnicmp(p, L"TEMP=", 5))
            {
                path.assign(p + 5);
                while (!path.empty() && path.back() == '\\')
                    path.pop_back();
                break;
            }
        }
        DestroyEnvironmentBlock((LPVOID)env);
    }
    return path;
}

// Determine the user temp directory (e.g. C:\Users\USER\AppData\Local\Temp)
static std::wstring getUserTempPath()
{
    std::wstring path(MAX_PATH + 2, 0);
    path.resize(GetTempPathW(path.size(), &path[0]));
    while (!path.empty() && path.back() == '\\')
        path.pop_back();
    return path;
}

// Determine the default install path (e.g. C:\Program Files\Private Internet Access)
static std::wstring getInstallPath()
{
#ifdef INSTALLER
    std::wstring path = getShellFolder(CSIDL_PROGRAM_FILES);
    path.push_back('\\');
    path += _T(PIA_PRODUCT_NAME);
    return path;
#endif
#ifdef UNINSTALLER
    return g_executablePath.substr(0, g_executablePath.find_last_of('\\'));
#endif
}


int main()
{
    g_instance = GetModuleHandle(NULL);

    g_executablePath = getExecutablePath();
    g_systemTempPath = getSystemTempPath();
    g_userTempPath = getSystemTempPath();

    g_installPath = getInstallPath();

    if (g_executablePath.empty() || g_systemTempPath.empty() || g_userTempPath.empty())
        return 8;

    Logger logger;

    if (HRESULT err = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))
        LOG("CoInitializeEx failed (%d)", err);

    auto argc = __argc;
    auto argv = __wargv;

    DWORD pid = 0;
    bool execute = false;

#ifdef INSTALLER
    // For now, the installer runs from its existing directory
    execute = true;
#endif

    // Parse arguments
    for (int i = 1; i < argc; i++)
    {
        if (!_wcsicmp(argv[i], L"/SILENT"))
        {
            g_silent = true;
            g_passive = true;
        }
        else if (!_wcsicmp(argv[i], L"/PASSIVE"))
        {
            g_passive = true;
        }
        else if (!_wcsicmp(argv[i], L"/PATH"))
        {
            if (++i >= argc)
            {
                LOG("Missing argument");
                return 2;
            }
            g_installPath.assign(argv[i]);
#ifdef UNINSTALLER
            if (!PathIsDirectory(g_installPath.c_str()))
            {
                LOG("Non-existent path argument: %ls", g_installPath);
                return 2;
            }
#endif
        }
        else if (!_wcsicmp(argv[i], L"/EXECUTE"))
        {
            execute = true;
        }
        else if (!_wcsicmp(argv[i], L"/WAITPID"))
        {
            if (++i >= argc)
            {
                LOG("Missing argument");
                return 2;
            }
            wchar_t* end;
            pid = wcstoul(argv[i], &end, 10);
            if (*end != 0 || !pid)
            {
                LOG("Unrecognized argument: %ls", argv[i]);
                return 2;
            }
        }
        else
        {
            LOG("Unrecognized argument: %ls", argv[i]);
            return 2;
        }
    }

    // If a PID was given, wait for it
    if (pid)
    {
        if (HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid))
        {
            if (WAIT_OBJECT_0 == WaitForSingleObject(process, INFINITE))
            {
                CloseHandle(process);
                // Continue executing normally
            }
            else
            {
                LOG("Failed to wait for process %d (%d)", pid, GetLastError());
                CloseHandle(process);
                return 3;
            }
        }
        else if (GetLastError() == ERROR_INVALID_PARAMETER)
        {
            // PID no longer valid = process exited
            // Continue executing normally
        }
        else
        {
            LOG("Failed to open process %d (%d)", pid, GetLastError());
            return 3;
        }
    }

    // Copy the executable to a temporary directory if needed
    if (!execute && !stringStartsWithCaseInsensitive(g_executablePath, g_systemTempPath + L"\\"))
    {
        // Grab the current PID, which the child process will wait on
        pid = GetCurrentProcessId();

        // Create a temporary file which we will overwrite with a copy of the
        // executable. Create this in the system temp directory which is not
        // user writable.
        std::wstring tempExecutablePath(MAX_PATH, 0);
        if (!GetTempFileNameW(g_systemTempPath.c_str(), L"pia", 0, &tempExecutablePath[0]))
            return 4;
        tempExecutablePath.resize(wcslen(&tempExecutablePath[0]));

        // Overwrite the temporary file with our executable
        if (!CopyFile(g_executablePath.c_str(), tempExecutablePath.c_str(), FALSE))
            return 4;

        // Schedule the temporary file to be removed on next reboot
        MoveFileEx(tempExecutablePath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

        // Construct a quoted command line for the child process
        std::wstring cmdline;
        appendQuotedArgument(cmdline, tempExecutablePath);
        if (g_silent)
            appendQuotedArgument(cmdline, L"/SILENT");
        appendQuotedArgument(cmdline, L"/WAITPID");
        appendQuotedArgument(cmdline, std::to_wstring(pid));
        appendQuotedArgument(cmdline, L"/PATH");
        appendQuotedArgument(cmdline, g_installPath);
        appendQuotedArgument(cmdline, L"/EXECUTE");

        // Launch the child process as a detached process and exit
        PROCESS_INFORMATION pi {0};
        STARTUPINFO si {0};
        si.cb = sizeof(si);

        if (!CreateProcessW(tempExecutablePath.c_str(), &cmdline[0], NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS | DETACHED_PROCESS, NULL, g_systemTempPath.c_str(), &si, &pi))
            return 4;

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }

#ifdef INSTALLER
    if (!initializePayload())
    {
        MessageBox(NULL, loadString(IDS_MB_MISSINGPAYLOAD).c_str(), loadString(IDS_MB_CAP_MISSINGPAYLOAD).c_str(), MB_ICONERROR | MB_OK);
        return 1;
    }
#endif

    // Acquire the necessary privileges
    adjustProcessTokenPrivileges({
        // Needed for registry operations
        { SE_BACKUP_NAME, SE_PRIVILEGE_ENABLED },
        { SE_RESTORE_NAME, SE_PRIVILEGE_ENABLED },
        // Needed to launch a process as non-admin
        { SE_INCREASE_QUOTA_NAME, SE_PRIVILEGE_ENABLED },
        { SE_IMPERSONATE_NAME, SE_PRIVILEGE_ENABLED },
    });

    // Run the actual installer & worker thread
    return Installer().run();
}
