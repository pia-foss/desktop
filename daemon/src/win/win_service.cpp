// Copyright (c) 2021 Private Internet Access, Inc.
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
#include "brand.h"
#include "../../../extras/installer/win/service_inl.h"
#include "../../../extras/installer/win/tun_inl.h"

#include <QString>
#include <QStringList>

static SERVICE_STATUS_HANDLE g_statusHandle;

static void serviceMain(int argc, wchar_t** argv);
static DWORD CALLBACK serviceCtrlHandler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context);
static bool reportStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0);


static void serviceMain(int argc, wchar_t** argv)
{
    if (HRESULT error = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))
        qFatal("CoInitializeEx failed with error 0x%08x.", error);
    if (!(g_statusHandle = ::RegisterServiceCtrlHandlerExW(PIA_SERVICE, serviceCtrlHandler, nullptr)))
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
        QObject::connect(&service, &Daemon::started, [&service]
        {
            reportStatus(SERVICE_RUNNING);

            // Do this after reporting the service as "running", it may wait for
            // the MSI service to start, and we don't want to wait on that while
            // SCM thinks we're still initializing
            service.handlePendingWinTunInstall();

            QCoreApplication::exec();
        });
        QObject::connect(&service, &Daemon::stopped, [] { QCoreApplication::quit(); });

        service.start();
        // start() completes when the service is stopped, due to the started()
        // signal being connected to QCoreApplication::exec()
        exitCode = 0;
    }
    catch (const Error& error)
    {
        qCritical() << error;
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
        { PIA_SERVICE, [](DWORD argc, LPWSTR *argv) { serviceMain(argc, argv); } },
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

int getSvcStatusExitCode(ServiceStatus status)
{
    switch(status)
    {
        case ServiceInstalled:
        case ServiceUpdated:
        case ServiceStarted:
        case ServiceAlreadyStarted:
        case ServiceStopped:
        case ServiceAlreadyStopped:
        case ServiceStoppedAndUninstalled:
        case ServiceUninstalled:
        case ServiceNotInstalled:
            // Successful codes
            return 0;
        default:
        case ServiceInstallFailed:
        case ServiceUpdateFailed:
        case ServiceStartFailed:
        case ServiceStopFailed:
        case ServiceUninstallFailed:
        case ServiceTimeout:
            // Failures
            return 2;
        case ServiceRebootNeeded:
            // Reboot required to complete operation.  This rarely occurs; it's
            // treated as a value between failure and success.
            return 1;
    }
}

int WinService::installService()
{
    // Install both the daemon and Wireguard services.  This is used by the
    // installer to roll back to a prior installation.  (It can also be used to
    // repair an installation, but usually it's recommended to just re-run the
    // installer.)
    auto daemonStatus = ::installDaemonService(qUtf16Printable(QCoreApplication::applicationFilePath()));
    ServiceStatus wireguardStatus{ServiceStatus::ServiceInstalled};
    // Install the WireGuard service only if supported
    if(isWintunSupported())
    {
        wireguardStatus = ::installWireguardService(qUtf16Printable(Path::WireguardServiceExecutable),
                                                    qUtf16Printable(Path::DaemonDataDir));
    }
    return std::max(getSvcStatusExitCode(daemonStatus), getSvcStatusExitCode(wireguardStatus));
}

int WinService::uninstallService()
{
    // Uninstall both the daemon and Wireguard services.  This command isn't
    // really used - the installer/uninstaller don't use it, and it's not often
    // used for troubleshooting - but it's here in case something really goes
    // wrong that requires manual repair.
    auto daemonStatus = ::uninstallService(g_daemonServiceParams.pName);
    auto wireguardStatus = ::uninstallService(g_wireguardServiceParams.pName);
    return std::max(getSvcStatusExitCode(daemonStatus), getSvcStatusExitCode(wireguardStatus));
}

int WinService::startService()
{
    return ::startService(g_daemonServiceParams.pName);
}

int WinService::stopService()
{
    return ::stopService(g_daemonServiceParams.pName);
}


void WinService::stop()
{
    reportStatus(SERVICE_STOP_PENDING);
    WinDaemon::stop();
}
