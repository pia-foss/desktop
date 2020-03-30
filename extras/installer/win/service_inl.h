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

#ifndef SERVICE_INL_H
#define SERVICE_INL_H

#include <utility>
#include <string>
#include <Windows.h>

#define PIA_SERVICE L"" BRAND_WINDOWS_SERVICE_NAME

class ServiceHandle
{
public:
    ServiceHandle() : _handle{nullptr} {}
    explicit ServiceHandle(SC_HANDLE handle) : _handle(handle) {}

    ServiceHandle(ServiceHandle &&other) : ServiceHandle{} {*this = std::move(other);}
    ServiceHandle &operator=(ServiceHandle &&other)
    {
        std::swap(_handle, other._handle);
        return *this;
    }

    ~ServiceHandle() { close(); }

private:
    ServiceHandle(const ServiceHandle&) = delete;
    ServiceHandle &operator=(const ServiceHandle&) = delete;

public:
    void reset(SC_HANDLE handle) {close(); _handle = handle;}
    void close()
    {
        if(_handle)
        {
            ::CloseServiceHandle(_handle);
            _handle = nullptr;
        }
    }
    operator SC_HANDLE() const { return _handle; }

private:
    SC_HANDLE _handle;
};

enum ServiceStatus
{
    ServiceInstalled = 10,
    ServiceInstallFailed,
    ServiceUpdated,
    ServiceUpdateFailed,

    ServiceStarted = 20,
    ServiceStartFailed,
    ServiceAlreadyStarted,

    ServiceStopped = 30,
    ServiceStopFailed,
    ServiceAlreadyStopped,

    ServiceStoppedAndUninstalled = 40,
    ServiceUninstalled,
    ServiceUninstallFailed,
    ServiceNotInstalled,

    ServiceRebootNeeded = 50,
    ServiceTimeout,
};

ServiceStatus startService(SC_HANDLE service, int timeoutMs);
ServiceStatus startService(LPCWSTR pSvcName);
ServiceStatus stopService(SC_HANDLE service);
ServiceStatus stopService(LPCWSTR pSvcName);

struct ServiceParams
{
    enum Flags : int
    {
        AutoRestart = 1<<0, // Configure the service to restart automatically
        UnrestrictedSid = 1<<1, // Set the SID type to SERVICE_SID_TYPE_UNRESTRICTED
    };
    LPCWSTR pName;  // Internal name of service
    LPCWSTR pDesc;  // Display name
    DWORD startType;    // Start type - see dwStartType of CreateServiceW()
    int flags;  // Extra flags - Flags above
};

extern const ServiceParams g_daemonServiceParams;
extern const ServiceParams g_wireguardServiceParams;

std::wstring quotePath(LPCWSTR path);
ServiceStatus installService(LPCWSTR command, const ServiceParams &params);
ServiceStatus uninstallService(LPCWSTR pSvcName);

// Install the daemon service (uses the daemon executable path and the daemon
// service params)
ServiceStatus installDaemonService(LPCWSTR daemonPath);
// Install the Wireguard service (uses the wireguard executable path, data
// directory path (for the config file), and wireguard service params)
ServiceStatus installWireguardService(LPCWSTR wireguardPath, LPCWSTR dataDirPath);

#endif
