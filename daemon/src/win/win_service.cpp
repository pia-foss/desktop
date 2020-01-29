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

#include "common.h"
#line SOURCE_FILE("win/win_service.cpp")

#include "win_service.h"
#include "path.h"
#include "win.h"
#include "version.h"
#include "brand.h"

#include <QString>
#include <QStringList>

#define SERVICE_LOG qInfo
#include "../../../extras/installer/win/service.inl"

static SERVICE_STATUS_HANDLE g_statusHandle;

static void serviceMain(int argc, wchar_t** argv);
static DWORD CALLBACK serviceCtrlHandler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context);
static bool reportStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0);


static void serviceMain(int argc, wchar_t** argv)
{
    if (HRESULT error = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))
        qFatal("CoInitializeEx failed with error 0x%08x.", error);
    if (!(g_statusHandle = ::RegisterServiceCtrlHandlerExW(L"" SERVICE_NAME, serviceCtrlHandler, nullptr)))
        qFatal("Unable to register service control handler");
    reportStatus(SERVICE_START_PENDING);

    DWORD exitCode = 1;
    try
    {
        Path::initializePreApp();

        // Note: argc and argv are actually picked up with GetCommandLine()
        // on Windows to get the proper unicode, so pass dummy values here.
        int c = 1; char* v = "";
        QCoreApplication app(c, &v);

        Path::initializePostApp();
        Logger logSingleton{Path::DaemonLogFile};

        WinService service;
        QObject::connect(&service, &Daemon::started, [] { reportStatus(SERVICE_RUNNING); QCoreApplication::exec(); });
        QObject::connect(&service, &Daemon::stopped, [] { QCoreApplication::quit(); });

        service.start();
        // start() completes when the service is stopped, due to the started()
        // signal being connected to QCoreApplication::exec()
        exitCode = service.exitCode();
    }
    catch (const Error& error)
    {
        qCritical(error);
        reportStatus(SERVICE_STOPPED, 1);
    }

    // It's important that we only report SERVICE_STOPPED _after_ the WinService
    // object is destroyed.  The WinDaemon destructor deletes WFP objects, and
    // the SCM may terminate the process as soon as we report that the service
    // is stopped.
    reportStatus(SERVICE_STOPPED, exitCode);
}

static DWORD CALLBACK serviceCtrlHandler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context)
{
    (void)eventType;
    (void)eventData;
    (void)context;
    switch (control)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        reportStatus(SERVICE_STOP_PENDING);
        QMetaObject::invokeMethod(g_service, &WinService::stop);
        return NO_ERROR;

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

static bool reportStatus(DWORD state, DWORD exitCode, DWORD waitHint)
{
    FUNCTION_LOGGING_CATEGORY("win.service");

    static SERVICE_STATUS status {
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_START_PENDING,
        0,
        NO_ERROR, 0,
        0, 0
    };
    if (status.dwCurrentState == SERVICE_STOPPED)
        return false;
    status.dwCurrentState = state;
    if (state == SERVICE_RUNNING)
        status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    else
        status.dwControlsAccepted = 0;
    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
        status.dwCheckPoint = 0;
    else
        ++status.dwCheckPoint;
    status.dwWin32ExitCode = exitCode == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
    status.dwServiceSpecificExitCode = exitCode;
    status.dwWaitHint = waitHint;
    if (!::SetServiceStatus(g_statusHandle, &status))
    {
        qWarning() << "Failed to set service status:" << qt_error_string();
        return false;
    }
    return true;
}

WinService::RunResult WinService::tryRun()
{
    static SERVICE_TABLE_ENTRYW table[] =
    {
        { L"" SERVICE_NAME, [](DWORD argc, LPWSTR *argv) { serviceMain(argc, argv); } },
        { NULL, NULL }
    };

    HANDLE out = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (out != INVALID_HANDLE_VALUE && out != NULL)
        return RunningAsConsole;

    if (::StartServiceCtrlDispatcherW(table))
        return SuccessfullyLaunched;
    else
    {
        switch (DWORD error = ::GetLastError())
        {
        default:
            qCritical() << "Unknown service start error:" << qt_error_string(error);
            // fallthrough
        case ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
            return RunningAsConsole;
        case ERROR_SERVICE_ALREADY_RUNNING:
            return AlreadyRunning;
        }
    }
}

int WinService::installService()
{
    return ::installService(qUtf16Printable(QCoreApplication::applicationFilePath()));
}

int WinService::uninstallService()
{
    return ::uninstallService();
}

int WinService::startService()
{
    return ::startService();
}

int WinService::stopService()
{
    return ::stopService();
}


void WinService::stop()
{
    reportStatus(SERVICE_STOP_PENDING);
    WinDaemon::stop();
}
