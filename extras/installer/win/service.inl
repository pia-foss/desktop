#ifndef SERVICE_INL
#define SERVICE_INL
#pragma once

#ifndef SERVICE_NAME
#define SERVICE_NAME BRAND_WINDOWS_SERVICE_NAME
#endif
#ifndef SERVICE_DISPLAY_NAME
#define SERVICE_DISPLAY_NAME PIA_PRODUCT_NAME " Service"
#endif
#ifndef SERVICE_START_TYPE
#define SERVICE_START_TYPE   SERVICE_AUTO_START
#endif
#ifndef SERVICE_DEPENDENCIES
#define SERVICE_DEPENDENCIES "\0" // null-separated list
#endif
#ifndef SERVICE_ACCOUNT
#define SERVICE_ACCOUNT      NULL // LocalSystem
//#define SERVICE_ACCOUNT      L"NT AUTHORITY\\LocalService"
//#define SERVICE_ACCOUNT      L"NT AUTHORITY\\NetworkService"
#endif
#ifndef SERVICE_PASSWORD
#define SERVICE_PASSWORD     NULL
#endif
#ifndef SERVICE_TIMEOUT
#define SERVICE_TIMEOUT      30000
#endif
#ifndef SERVICE_LOG
#define SERVICE_LOG(...) ((void)0)
#endif
#ifndef FUNCTION_LOGGING_CATEGORY
#define FUNCTION_LOGGING_CATEGORY(cat) ((void)0)
#endif

#include <windows.h>
#include "service_inl.h"

#pragma comment(lib, "advapi32.lib")

class ServiceTimeoutScope
{
public:
    explicit ServiceTimeoutScope(int timeoutMs)
    {
        _first = (_timeRemaining == INT_MAX);
        if(_first)
            _timeRemaining = timeoutMs;
    }
    ~ServiceTimeoutScope() { if (_first) _timeRemaining = INT_MAX; }
    static bool check(int ms) { if (_timeRemaining <= 0) return true; _timeRemaining -= ms; return false; }
private:
    bool _first;
    static int _timeRemaining;
};
int ServiceTimeoutScope::_timeRemaining = INT_MAX;

// Declare a service entry point with a specific timeout.  The timeout given is
// only applied if this is the first service entry point on the stack.
#define SERVICE_ENTRY_POINT_TIMEOUT(timeoutMs) FUNCTION_LOGGING_CATEGORY("win.service"); ServiceTimeoutScope scopeTimeout{timeoutMs}
#define SERVICE_ENTRY_POINT    SERVICE_ENTRY_POINT_TIMEOUT(SERVICE_TIMEOUT)

#define sleepAndQueryServiceStatus(service, status) do { if (ServiceTimeoutScope::check(300)) goto timeout; Sleep(300); if (!QueryServiceStatus(service, status)) goto error; } while (false)
#define openManager(flags, errorStatus) \
    ServiceHandle manager { OpenSCManager(NULL, NULL, (flags)) }; \
    if (manager == NULL) \
    { \
        SERVICE_LOG("Unable to open Service Control Manager (%d)", GetLastError()); \
        return errorStatus; \
    } else ((void)0)
#define openService(flags, errorStatus) \
    ServiceHandle service { OpenService(manager, L"" SERVICE_NAME, (flags)) }; \
    if (service == NULL) \
    { \
        SERVICE_LOG("Unable to open service (%d)", GetLastError()); \
        return errorStatus; \
    } else ((void)0)

ServiceStatus startService(SC_HANDLE service, int timeoutMs)
{
    SERVICE_ENTRY_POINT_TIMEOUT(timeoutMs);

    if (!StartService(service, 0, NULL))
        goto error;

    SERVICE_STATUS status;
    do {
        sleepAndQueryServiceStatus(service, &status);
    } while (status.dwCurrentState == SERVICE_START_PENDING);
    if (status.dwCurrentState != SERVICE_RUNNING)
    {
        SERVICE_LOG("Service didn't start");
        return ServiceStartFailed;
    }
    SERVICE_LOG("Service successfully started");
    return ServiceStarted;

timeout:
    SERVICE_LOG("Service start timed out");
    return ServiceTimeout;

error:
    switch (DWORD err = GetLastError())
    {
    case ERROR_SERVICE_ALREADY_RUNNING:
        SERVICE_LOG("Service already running");
        return ServiceAlreadyStarted;
    case ERROR_SERVICE_MARKED_FOR_DELETE:
        SERVICE_LOG("Service marked for deletion; reboot needed");
        return ServiceRebootNeeded;
    default:
        SERVICE_LOG("Failed to start service (%d)", err);
        return ServiceStartFailed;
    }
}

static ServiceStatus startService()
{
    SERVICE_ENTRY_POINT;

    openManager(SC_MANAGER_CONNECT, ServiceStartFailed);
    openService(SERVICE_START | SERVICE_QUERY_STATUS, ServiceStartFailed);

    SERVICE_LOG("Starting service");
    return startService(service, SERVICE_TIMEOUT);
}

ServiceStatus stopService(SC_HANDLE service)
{
    SERVICE_ENTRY_POINT;

    SERVICE_LOG("Stopping service");

    SERVICE_STATUS status;
    for (;;)
    {
        SERVICE_LOG("Sending stop signal to service");
        if (ControlService(service, SERVICE_CONTROL_STOP, &status))
        {
            // Wait for stop operation to finish (include SERVICE_RUNNING in case
            // the service is slow at reporting its status).
            while (status.dwCurrentState == SERVICE_RUNNING || status.dwCurrentState == SERVICE_STOP_PENDING)
                sleepAndQueryServiceStatus(service, &status);

            // Check if the service actually stopped
            if (status.dwCurrentState == SERVICE_STOPPED)
            {
                SERVICE_LOG("Service has stopped");
                return ServiceStopped;
            }
            else
            {
                SERVICE_LOG("Service stop failed; in state %d", status.dwCurrentState);
                return ServiceStopFailed;
            }
        }
        else
        {
            switch (GetLastError())
            {
            case ERROR_INVALID_SERVICE_CONTROL:
            case ERROR_SERVICE_CANNOT_ACCEPT_CTRL:
                // Wait for the service to enter a usable state
                while (status.dwCurrentState == SERVICE_START_PENDING || status.dwCurrentState == SERVICE_STOP_PENDING)
                    sleepAndQueryServiceStatus(service, &status);
                // If the service is now running, try again to send it a stop signal
                if (status.dwCurrentState == SERVICE_RUNNING)
                    continue;
                else if (status.dwCurrentState != SERVICE_STOPPED)
                {
                    SERVICE_LOG("Service in unrecognized state %d", status.dwCurrentState);
                    return ServiceStopFailed;
                }
                // fallthrough (SERVICE_STOPPED)
            case ERROR_SERVICE_NOT_ACTIVE:
                SERVICE_LOG("Service already stopped");
                return ServiceAlreadyStopped;
            case ERROR_SERVICE_MARKED_FOR_DELETE:
                SERVICE_LOG("Service marked for deletion");
                return ServiceRebootNeeded;
            timeout:
                SERVICE_LOG("Service stop timed out");
                return ServiceTimeout;
            default:
            error:
                SERVICE_LOG("Failed to stop service (%d)", GetLastError());
                return ServiceStopFailed;
            }
        }
    }
}

static ServiceStatus stopService()
{
    SERVICE_ENTRY_POINT;

    openManager(SC_MANAGER_CONNECT, ServiceStopFailed);

    // Open the service manually to catch ServiceNotInstalled properly
    ServiceHandle service { OpenService(manager, L"" SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS) };
    if (service == NULL)
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            SERVICE_LOG("Service not installed");
            return ServiceNotInstalled;
        }
        SERVICE_LOG("Unable to open service (%d)", err);
        return ServiceStopFailed;
    }
    return stopService(service);
}

static bool setupAutoRestart () {
    // We need full access for setting up the recovery options
    openManager(SC_MANAGER_ALL_ACCESS, false);
    openService(SERVICE_ALL_ACCESS, false);

    _SERVICE_FAILURE_ACTIONSW autoRestart;
    // Reset failures if no failures for 60 seconds
    autoRestart.dwResetPeriod = (DWORD)60;
    autoRestart.cActions = (DWORD)1;
    autoRestart.lpCommand = L"";
    autoRestart.lpRebootMsg = L"";
    struct _SC_ACTION actions[1];
    autoRestart.lpsaActions = actions;
    autoRestart.lpsaActions[0].Delay = (DWORD)0;
    autoRestart.lpsaActions[0].Type = SC_ACTION_RESTART;

    if(!ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &autoRestart)) {
        SERVICE_LOG("Failed to configure auto restart service (%d)", GetLastError());
        return false;
    }

    return true;
}

static ServiceStatus installService(LPCWSTR path)
{
    SERVICE_ENTRY_POINT;

    // Quote the path
    std::wstring quotedPath;
    quotedPath.push_back('"');
    quotedPath.append(path);
    quotedPath.push_back('"');

    openManager(SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE, ServiceInstallFailed);

    // First try to create the service
    SERVICE_LOG("Installing service");
    {
        ServiceHandle service {
            CreateServiceW(
                        manager,                   // SCManager database
                        L"" SERVICE_NAME,          // Name of service
                        L"" SERVICE_DISPLAY_NAME,  // Name to display
                        SERVICE_QUERY_STATUS,      // Desired access
                        SERVICE_WIN32_OWN_PROCESS, // Service type
                        SERVICE_START_TYPE,        // Service start type
                        SERVICE_ERROR_NORMAL,      // Error control type
                        quotedPath.c_str(),        // Service's binary
                        NULL,                      // No load ordering group
                        NULL,                      // No tag identifier
                        L"" SERVICE_DEPENDENCIES,  // Dependencies
                        SERVICE_ACCOUNT,           // Service running account
                        SERVICE_PASSWORD           // Password of the account
                        )
        };
        if (service != NULL)
        {
            SERVICE_LOG("Successfully created service");

            setupAutoRestart();

            return ServiceInstalled;
        }
    }

    DWORD err = GetLastError();
    if (err == ERROR_SERVICE_EXISTS)
    {
        // Update existing service
        SERVICE_LOG("Updating existing service");

        openService(SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG, ServiceInstallFailed);

        if (!ChangeServiceConfigW(
                    service,
                    SERVICE_WIN32_OWN_PROCESS,
                    SERVICE_START_TYPE,
                    SERVICE_ERROR_NORMAL,
                    quotedPath.c_str(),
                    NULL,
                    NULL,
                    L"" SERVICE_DEPENDENCIES,
                    SERVICE_ACCOUNT,
                    SERVICE_PASSWORD,
                    L"" SERVICE_DISPLAY_NAME))
        {
            SERVICE_LOG("Failed to update service (%d)", GetLastError());
            return ServiceUpdateFailed;
        }

        setupAutoRestart();

        SERVICE_LOG("Successfully updated service");
        return ServiceUpdated;
    }
    if (err == ERROR_SERVICE_MARKED_FOR_DELETE)
    {
        SERVICE_LOG("Service marked for deletion; reboot needed");
        return ServiceRebootNeeded;
    }

    SERVICE_LOG("Failed to create service (%d)", err);
    return ServiceInstallFailed;
}

static ServiceStatus uninstallService()
{
    SERVICE_ENTRY_POINT;

    openManager(SC_MANAGER_CONNECT, ServiceUninstallFailed);
    ServiceHandle service { OpenService(manager, L"" SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE) };
    if (service == NULL)
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            SERVICE_LOG("Service not installed");
            return ServiceNotInstalled;
        }
        SERVICE_LOG("Unable to open service (%d)", err);
        return ServiceUninstallFailed;
    }

    ServiceStatus stopStatus = stopService(service);
    if (stopStatus == ServiceStopFailed)
    {
        return ServiceUninstallFailed;
    }
    SERVICE_LOG("Uninstalling service");
    if (!DeleteService(service))
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_MARKED_FOR_DELETE)
        {
            SERVICE_LOG("Service already marked for deletion");
            return ServiceRebootNeeded;
        }
        SERVICE_LOG("Failed to uninstall service (%d)", err);
        return ServiceUninstallFailed;
    }

    SERVICE_LOG("Successfully uninstalled service");
    if (stopStatus == ServiceStopped)
        return ServiceStoppedAndUninstalled;
    else
        return ServiceUninstalled;
}

#undef SERVICE_ENTRY_POINT
#undef sleepAndQueryServiceStatus
#undef openManager
#undef openService

#endif // SERVICE_INL
