#pragma once

// TAP driver installation/uninstallation.  The setupapi helpers this uses
// (getSystemRoot, g_oemInfs, ScopedNonInteractive) are shared with the callout
// driver code and defined in callout.inl, which precedes this file in the
// translation units that build these inline files.

#ifndef TAP_HARDWARE_ID
#define TAP_HARDWARE_ID "tap-pia-0901"
#endif
#ifndef TAP_DRIVER_VERSION
// Version of the TAP driver build shipped with this build of PIA - used to
// detect whether to uninstall the driver before an update.  Must match the
// DriverVersion field in OemVista.inf.  (Used as initializer for a
// WinDriverVersion)
#define TAP_DRIVER_VERSION 9, 27, 0, 0
#endif
#ifndef TAP_DESCRIPTION
#define TAP_DESCRIPTION NULL
#endif
#ifndef TAP_LOG
#define TAP_LOG(...) ((void)0)
#endif
#ifndef FUNCTION_LOGGING_CATEGORY
#define FUNCTION_LOGGING_CATEGORY(cat) ((void)0)
#endif

#include "tap_inl.h"
#include "service_inl.h"

#include <newdev.h>

#pragma comment(lib, "newdev.lib")

struct DeviceClass
{
    GUID guid;
    LPCWSTR name;
};

// "Net" {4D36E972-E325-11CE-BFC1-08002BE10318}
static const DeviceClass CLASS_Net {
    { 0x4d36e972, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } },
    L"Net",
};

// Add an extra null at the end to make this a double null terminated list
static const wchar_t g_hardwareIdList[] = L"" TAP_HARDWARE_ID "\0";


// callout.inl defines ScopedNonInteractive but undefines this convenience
// macro at its end, so define it here for the TAP code.
#define NonInteractiveScope(enable) ScopedNonInteractive nonInteractiveBlock(nonInteractive); (void)nonInteractiveBlock

static DriverStatus updateTapDriver(LPCWSTR inf, bool forceUpdate, bool nonInteractive = false)
{
    FUNCTION_LOGGING_CATEGORY("win.tap");

    NonInteractiveScope(nonInteractive);

    BOOL reboot = false;
    if (UpdateDriverForPlugAndPlayDevicesW(NULL, g_hardwareIdList, inf, (forceUpdate ? INSTALLFLAG_FORCE : 0) | (nonInteractive ? INSTALLFLAG_NONINTERACTIVE : 0), &reboot))
    {
        TAP_LOG("Successfully updated device driver");
        return reboot ? DriverUpdatedReboot : DriverUpdated;
    }
    else
    {
        switch (DWORD err = GetLastError())
        {
        case ERROR_NO_SUCH_DEVINST:
            TAP_LOG("No devices to update");
            return DriverNothingToUpdate;
        case ERROR_NO_MORE_ITEMS:
            TAP_LOG("Device driver is already up to date");
            return DriverUpdateNotNeeded;
        case ERROR_AUTHENTICODE_PUBLISHER_NOT_TRUSTED:
        case ERROR_AUTHENTICODE_TRUST_NOT_ESTABLISHED:
            TAP_LOG("Device driver publisher not trusted");
            return DriverUpdateDisallowed;
        default:
            TAP_LOG("Unable to update device driver (%d)", err);
            return DriverUpdateFailed;
            // TODO: Permission errors
        }
    }
}

DriverStatus installTapDriver(LPCWSTR inf, bool alwaysCreateNew, bool forceUpdate, bool nonInteractive)
{
    FUNCTION_LOGGING_CATEGORY("win.tap");

    NonInteractiveScope(nonInteractive);

    DriverStatus status;

    if (!alwaysCreateNew)
    {
        // Attempt to update any existing devices
        status = updateTapDriver(inf, forceUpdate, nonInteractive);
        // Only proceed if the update failed due to no devices existing
        if (status != DriverNothingToUpdate)
            return status;
    }

    status = DriverInstallFailed;

    // Create a new device instance of our hardware ID

    HDEVINFO devInfoSet = SetupDiCreateDeviceInfoList(&CLASS_Net.guid, NULL);
    if (devInfoSet != INVALID_HANDLE_VALUE)
    {
        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(devInfoData);
        if (SetupDiCreateDeviceInfoW(devInfoSet, CLASS_Net.name, &CLASS_Net.guid, TAP_DESCRIPTION, 0, DICD_GENERATE_ID, &devInfoData))
        {
            if (SetupDiSetDeviceRegistryPropertyW(devInfoSet, &devInfoData, SPDRP_HARDWAREID, (const BYTE*)g_hardwareIdList, sizeof(g_hardwareIdList)))
            {
                if (SetupDiCallClassInstaller(DIF_REGISTERDEVICE, devInfoSet, &devInfoData))
                {
                    TAP_LOG("Successfully created device");
                    switch (updateTapDriver(inf, true, nonInteractive))
                    {
                    case DriverUpdateNotNeeded:
                    case DriverUpdated:
                        status = DriverInstalled;
                        break;
                    case DriverUpdatedReboot:
                        status = DriverInstalledReboot;
                        break;
                    case DriverUpdateDisallowed:
                        status = DriverInstallDisallowed;
                        // fallthrough
                    default:
                        if (!SetupDiCallClassInstaller(DIF_REMOVE, devInfoSet, &devInfoData))
                            TAP_LOG("Unable to remove device (%d)", GetLastError());
                        break;
                    }
                }
                else
                    TAP_LOG("Unable to install device (%d)", GetLastError());
            }
            else
                TAP_LOG("Unable to set device hardware ID (%d)", GetLastError());
        }
        else
            TAP_LOG("Unable to create device info (%d)", GetLastError());
        SetupDiDestroyDeviceInfoList(devInfoSet);
    }
    else
        TAP_LOG("Unable to create device info list (%d)", GetLastError());

    return status;
}


bool uninstallTapDriverInf()
{
    // Additionally, search the C:\Windows\inf directory for the installed oem*.inf
    std::wstring infPath = getSystemRoot();
    if (!infPath.empty())
    {
        infPath += L"\\inf\\oem*.inf";
        WIN32_FIND_DATA find;
        HANDLE it = FindFirstFileW(infPath.c_str(), &find);
        infPath.resize(infPath.size() - 8, 0);
        if (it != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (find.nFileSizeHigh || find.nFileSizeLow >= 100000)
                    continue; // Skip any bizarre large files
                HANDLE file = CreateFileW((infPath + find.cFileName).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                if (file != INVALID_HANDLE_VALUE)
                {
                    std::string data;
                    data.resize(find.nFileSizeLow, 0);
                    DWORD bytesRead = 0;
                    if (ReadFile(file, &data[0], find.nFileSizeLow, &bytesRead, NULL) && bytesRead == find.nFileSizeLow)
                    {
                        if (data.find(TAP_HARDWARE_ID ".sys") != data.npos)
                        {
                            g_oemInfs.insert(find.cFileName);
                        }
                    }
                    CloseHandle(file);
                }
            } while (FindNextFile(it, &find));
            FindClose(it);
        }
    }

    // Uninstall any found driver packages
    bool infFailures = false;
    for (const auto& oemInf : g_oemInfs)
    {
        TAP_LOG("Uninstalling driver package");
        if (!SetupUninstallOEMInfW(oemInf.c_str(), 0 /*SUOI_FORCEDELETE*/, NULL))
        {
            TAP_LOG("Unable to uninstall driver package (%d)", GetLastError());
            infFailures = true;
        }
    }

    return !infFailures;
}

// Uninstall the TAP driver
// - removeInf - whether to also remove the INF file from C:\Windows\INF\oem*.inf
// - onlyDifferentVersion - only uninstall if any version other than the version
//   shipped with this build of PIA is installed.  Should be used with
//   removeInf=true so the old INF files are removed.  (If only the shipped
//   version is installed, nothing happens and DriverNothingToUninstall is
//   returned)
DriverStatus uninstallTapDriver(bool removeInf, bool onlyDifferentVersion)
{
    FUNCTION_LOGGING_CATEGORY("win.tap");

    const WinDriverVersion shippedVersion{TAP_DRIVER_VERSION};

    // Get all devices in the class "Net"
    bool devicesExisted = false, deviceFailures = false;
    HDEVINFO devInfoSet = SetupDiGetClassDevsExW(&CLASS_Net.guid, NULL, NULL, 0, NULL, NULL, NULL);
    if (devInfoSet != INVALID_HANDLE_VALUE)
    {
        // Iterate over the device set
        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(devInfoData);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfoSet, i, &devInfoData); i++)
        {
            // Check the hardware ID list of the device
            BYTE buffer[sizeof(g_hardwareIdList)];
            DWORD dataType, actualSize;
            if (!SetupDiGetDeviceRegistryPropertyW(devInfoSet, &devInfoData, SPDRP_HARDWAREID, &dataType, buffer, sizeof(buffer), &actualSize))
                continue;
            if (dataType != REG_MULTI_SZ || actualSize != sizeof(g_hardwareIdList) || memcmp(buffer, g_hardwareIdList, sizeof(g_hardwareIdList)))
                continue;

            SP_DRVINFO_DATA_W drvInfo{};
            drvInfo.cbSize = sizeof(drvInfo);

            // Enumerate the drivers for this device.  There could be more than
            // one version installed.  Skip uninstall only if exactly 1 version
            // is installed, and it is the shipped version.
            if(::SetupDiBuildDriverInfoList(devInfoSet, &devInfoData, SPDIT_COMPATDRIVER))
            {
                WinDriverVersion installedVersion{0, 0, 0, 0};
                DWORD idx = 0;
                while(::SetupDiEnumDriverInfoW(devInfoSet, &devInfoData, SPDIT_COMPATDRIVER, idx, &drvInfo))
                {
                    installedVersion = {drvInfo.DriverVersion};
                    TAP_LOG("Driver %d - version %s", idx, installedVersion.printable().c_str());
                    ++idx;
                }

                DWORD err = ::GetLastError();
                if(err == ERROR_NO_MORE_ITEMS)
                {
                    TAP_LOG("Enumerated %d items", idx);
                    // The driver matches if there was exactly 1 version, and it
                    // is the version shipped with this build
                    if(idx == 1 && installedVersion == shippedVersion && onlyDifferentVersion)
                    {
                        TAP_LOG("Not uninstalling this driver, only the shipped version %s is installed",
                                shippedVersion.printable().c_str());
                        // Since we're skipping the uninstall, do not remove INF
                        // files
                        removeInf = false;
                        continue;
                    }
                }
                else
                    TAP_LOG("Failed after %d items - %d", idx, err);
            }
            else
            {
                TAP_LOG("Failed to build driver list - %d", ::GetLastError());
            }

            // Read out the InfPath key from the device's associated registry key
            HKEY key = SetupDiOpenDevRegKey(devInfoSet, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, GENERIC_READ);
            if (key != INVALID_HANDLE_VALUE)
            {
                wchar_t infPath[MAX_PATH];
                actualSize = sizeof(infPath);
                if (RegQueryValueExW(key, L"InfPath", NULL, &dataType, (LPBYTE)(LPWSTR)infPath, &actualSize) && dataType == REG_SZ)
                {
                    std::wstring oemInf(infPath, actualSize / 2);
                    while (!oemInf.empty() && oemInf.back() == '\0')
                        oemInf.pop_back();
                    g_oemInfs.insert(std::move(oemInf));
                }
                RegCloseKey(key);
            }

            devicesExisted = true;

            // Invoke the DIF_REMOVE action on the device
            if (!SetupDiCallClassInstaller(DIF_REMOVE, devInfoSet, &devInfoData))
            {
                TAP_LOG("Failed to remove device (%d)", GetLastError());
                deviceFailures = true;
            }
        }
        SetupDiDestroyDeviceInfoList(devInfoSet);
    }

    bool infFailures = removeInf && !uninstallTapDriverInf();

    if (!devicesExisted)
        return DriverNothingToUninstall;
    if (deviceFailures)
        return DriverUninstallFailed;

    return DriverUninstalled;
}

// WFP callout driver installation/uninstallation.  (Shares setupapi utilities
// with the TAP installation code.)

#undef NonInteractiveScope
