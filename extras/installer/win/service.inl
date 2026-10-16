#ifndef SERVICE_INL
#define SERVICE_INL
#pragma once

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
#include <VersionHelpers.h>
#include "service_inl.h"
#include "common_inl.h"
#include "brand.h"
#include "product.h"

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
#define openService(svcName, flags, errorStatus) \
    ServiceHandle service { OpenService(manager, svcName, (flags)) }; \
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

ServiceStatus startService(LPCWSTR pSvcName)
{
    SERVICE_ENTRY_POINT;

    openManager(SC_MANAGER_CONNECT, ServiceStartFailed);
    openService(pSvcName, SERVICE_START | SERVICE_QUERY_STATUS, ServiceStartFailed);

    SERVICE_LOG("Starting service %ls", pSvcName);
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

ServiceStatus stopService(LPCWSTR pSvcName)
{
    SERVICE_ENTRY_POINT;

    openManager(SC_MANAGER_CONNECT, ServiceStopFailed);

    // Open the service manually to catch ServiceNotInstalled properly
    ServiceHandle service { OpenService(manager, pSvcName, SERVICE_STOP | SERVICE_QUERY_STATUS) };
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

const ServiceParams g_daemonServiceParams
{
    PIA_SERVICE,
    L"" PIA_PRODUCT_NAME " Service",
    SERVICE_AUTO_START,
    ServiceParams::Flags::AutoRestart
};

const ServiceParams g_wireguardServiceParams
{
    L"" BRAND_WINDOWS_WIREGUARD_SERVICE_NAME,
    L"" PIA_PRODUCT_NAME " WireGuard Service",
    SERVICE_DEMAND_START,
    ServiceParams::Flags::UnrestrictedSid
};

std::wstring quotePath(LPCWSTR path)
{
    std::wstring quoted;
    quoted += '"';
    quoted += path;
    quoted += '"';
    return quoted;
}

bool setExtraServiceConfig(const ServiceParams &params)
{
    static wchar_t empty_string[] = L"";
    // We need full access for setting up the recovery options
    openManager(SC_MANAGER_ALL_ACCESS, false);
    openService(params.pName, SERVICE_ALL_ACCESS, false);

    if(params.flags & ServiceParams::Flags::AutoRestart)
    {
        _SERVICE_FAILURE_ACTIONSW autoRestart;
        // Reset failures if no failures for 60 seconds
        autoRestart.dwResetPeriod = (DWORD)60;
        autoRestart.cActions = (DWORD)1;
        autoRestart.lpCommand = empty_string;
        autoRestart.lpRebootMsg = empty_string;
        struct _SC_ACTION actions[1];
        autoRestart.lpsaActions = actions;
        autoRestart.lpsaActions[0].Delay = (DWORD)0;
        autoRestart.lpsaActions[0].Type = SC_ACTION_RESTART;

        if(!ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &autoRestart))
        {
            WinErrorEx error{::GetLastError()};
            SERVICE_LOG("Failed to configure auto restart service %ls (0x%X) %s",
                        params.pName, error.code(), error.message().c_str());
            return false;
        }
    }

    if(params.flags & ServiceParams::Flags::UnrestrictedSid)
    {
        SERVICE_SID_INFO unrestrictedSid;
        unrestrictedSid.dwServiceSidType = SERVICE_SID_TYPE_UNRESTRICTED;
        BOOL configChange = ChangeServiceConfig2(service, SERVICE_CONFIG_SERVICE_SID_INFO, &unrestrictedSid);

        if(!configChange)
        {
            WinErrorEx err{::GetLastError()};
            SERVICE_LOG("Failed to set SID type for service %ls - 0x%X %s", params.pName,
                        err.code(), err.message().c_str());
            return false;
        }
    }

    return true;
}

ServiceStatus installService(LPCWSTR command, const ServiceParams &params)
{
    SERVICE_ENTRY_POINT;

    openManager(SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE, ServiceInstallFailed);

    // These parameters are the same for all services created by this function
    // (used when calling both CreateServiceW() and ChangeServiceConfigW())
    const DWORD serviceType = SERVICE_WIN32_OWN_PROCESS;
    const DWORD errorCtlType = SERVICE_ERROR_NORMAL;
    const LPCWSTR pSvcAccount = nullptr;    // nullptr = LocalSystem
    const LPCWSTR pSvcPassword = nullptr;   // No password
    const LPCWSTR pLoadOrderGroup = L"";     // No load order group (nullptr means "no change" for ChangeServiceConfigW())
    const LPCWSTR pDependencies = L"\0";     // No dependencies

    // First try to create the service
    SERVICE_LOG("Installing service %ls", params.pName);
    {
        ServiceHandle service {
            CreateServiceW(
                        manager,                   // SCManager database
                        params.pName,              // Name of service
                        params.pDesc,              // Name to display
                        SERVICE_QUERY_STATUS,      // Desired access
                        serviceType,               // Service type
                        params.startType,          // Service start type
                        errorCtlType,              // Error control type
                        command,                   // Service's binary
                        pLoadOrderGroup,           // No load ordering group
                        NULL,                      // No tag identifier
                        pDependencies,             // No dependencies
                        pSvcAccount,               // Service running account - LocalSystem
                        pSvcPassword               // Password of the account - none
                        )
        };
        if (service != NULL)
        {
            SERVICE_LOG("Successfully created service %ls", params.pName);

            if(!setExtraServiceConfig(params))
                return ServiceInstallFailed;

            SERVICE_LOG("Successfully configured service %ls", params.pName);

            return ServiceInstalled;
        }
    }

    WinErrorEx error{::GetLastError()};
    if (error.code() == ERROR_SERVICE_EXISTS)
    {
        // Update existing service
        SERVICE_LOG("Updating existing service %ls", params.pName);

        openService(params.pName, SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG, ServiceInstallFailed);

        if (!ChangeServiceConfigW(
                    service,
                    serviceType,
                    params.startType,
                    errorCtlType,
                    command,
                    pLoadOrderGroup,
                    NULL,
                    pDependencies,
                    pSvcAccount,
                    pSvcPassword,
                    params.pDesc))
        {
            WinErrorEx updateError{::GetLastError()};
            SERVICE_LOG("Failed to update service %ls (0x%X) %s", params.pName,
                        updateError.code(), updateError.message().c_str());
            return ServiceUpdateFailed;
        }

        if(!setExtraServiceConfig(params))
            return ServiceUpdateFailed;

        SERVICE_LOG("Successfully updated service %ls", params.pName);
        return ServiceUpdated;
    }
    if (error.code() == ERROR_SERVICE_MARKED_FOR_DELETE)
    {
        SERVICE_LOG("Service %ls marked for deletion; reboot needed", params.pName);
        return ServiceRebootNeeded;
    }

    SERVICE_LOG("Failed to create service %ls (0x%X) %s", params.pName,
                error.code(), error.message().c_str());
    return ServiceInstallFailed;
}

ServiceStatus uninstallService(LPCWSTR pSvcName)
{
    SERVICE_ENTRY_POINT;

    openManager(SC_MANAGER_CONNECT, ServiceUninstallFailed);
    ServiceHandle service { OpenService(manager, pSvcName, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE) };
    if (service == NULL)
    {
        WinErrorEx error{::GetLastError()};
        if (error.code() == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            SERVICE_LOG("Service %ls not installed", pSvcName);
            return ServiceNotInstalled;
        }
        SERVICE_LOG("Unable to open service %ls (0x%X) %s", pSvcName,
                    error.code(), error.message().c_str());
        return ServiceUninstallFailed;
    }

    ServiceStatus stopStatus = stopService(service);
    if (stopStatus == ServiceStopFailed)
    {
        return ServiceUninstallFailed;
    }
    SERVICE_LOG("Uninstalling service %ls", pSvcName);
    if (!DeleteService(service))
    {
        WinErrorEx error{::GetLastError()};
        if (error.code() == ERROR_SERVICE_MARKED_FOR_DELETE)
        {
            SERVICE_LOG("Service %ls already marked for deletion (0x%X) %s",
                        pSvcName, error.code(), error.message().c_str());
            return ServiceRebootNeeded;
        }
        SERVICE_LOG("Failed to uninstall service %ls (0x%X) %s", pSvcName,
                    error.code(), error.message().c_str());
        return ServiceUninstallFailed;
    }

    SERVICE_LOG("Successfully uninstalled service %ls", pSvcName);
    if (stopStatus == ServiceStopped)
        return ServiceStoppedAndUninstalled;
    else
        return ServiceUninstalled;
}

ServiceStatus installDaemonService(LPCWSTR daemonPath)
{
    return installService(quotePath(daemonPath).c_str(), g_daemonServiceParams);
}

ServiceStatus installWireguardService(LPCWSTR wireguardPath, LPCWSTR dataDirPath)
{
    // The startup command is:
    // "...\wgservice.exe" "...\data\wgpia0.conf" "BRAND_NAME"
    std::wstring svcCommand;
    svcCommand += '"';
    svcCommand += wireguardPath;
    svcCommand += L"\" \"";
    svcCommand += dataDirPath;
    svcCommand += L"\\wg" BRAND_CODE "0.conf\" \""
                  L"" BRAND_NAME L" WireGuard\"";
    return installService(svcCommand.c_str(), g_wireguardServiceParams);
}

// Uninstall the driver INF with the specified hardware ID.  Relies on
// getSystemRoot() and <setupapi.h> from callout.inl, which is included before
// service.inl in both translation units that build these inline files.
bool uninstallDriverInf(LPCWSTR hardwareId)
{
    // Additionally, search the C:\Windows\inf directory for the installed oem*.inf
    std::wstring infPath = getSystemRoot();
    if(!infPath.empty())
    {
        infPath += L"\\inf\\oem*.inf";
        WIN32_FIND_DATA find;
        HANDLE it = FindFirstFileW(infPath.c_str(), &find);
        infPath.resize(infPath.size() - 8, 0);
        if(it != INVALID_HANDLE_VALUE)
        {
            do
            {
                if(find.nFileSizeHigh || find.nFileSizeLow >= 100000)
                {
                    continue; // Skip any bizarre large files
                }
                HANDLE file =
                    CreateFileW((infPath + find.cFileName).c_str(), GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                if(file != INVALID_HANDLE_VALUE)
                {
                    std::string data;
                    data.resize(find.nFileSizeLow, 0);
                    DWORD bytesRead = 0;
                    if(ReadFile(file, &data[0], find.nFileSizeLow, &bytesRead, NULL) &&
                       bytesRead == find.nFileSizeLow)
                    {
                        std::wstring hwidW(hardwareId);
                        std::string hwidN(hwidW.begin(), hwidW.end());
                        if(data.find(hwidN + ".sys") != data.npos)
                        {
                            g_oemInfs.insert(find.cFileName);
                        }
                    }
                    CloseHandle(file);
                }
            } while(FindNextFile(it, &find));
            FindClose(it);
        }
    }

    // Uninstall any found driver packages
    bool infFailures = false;
    for(const auto &oemInf : g_oemInfs)
    {
        SERVICE_LOG("Uninstalling driver package");
        if(!SetupUninstallOEMInfW(oemInf.c_str(), 0 /*SUOI_FORCEDELETE*/, NULL))
        {
            SERVICE_LOG("Unable to uninstall driver package (%d)", GetLastError());
            infFailures = true;
        }
    }

    return !infFailures;
}

#undef SERVICE_ENTRY_POINT
#undef sleepAndQueryServiceStatus
#undef openManager
#undef openService

#endif // SERVICE_INL
