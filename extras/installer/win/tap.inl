#ifndef TAP_INL
#define TAP_INL
#pragma once

#ifndef TAP_HARDWARE_ID
#define TAP_HARDWARE_ID "tap-pia-0901"
#endif
#ifndef TAP_DRIVER_VERSION
// Version of the TAP driver build shipped with this build of PIA - used to
// detect whether to uninstall the driver before an update.  Must match the
// DriverVersion field in OemVista.inf.  (Used as initializer for a
// WinDriverVersion)
#define TAP_DRIVER_VERSION 9, 24, 2, 601
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

#ifdef _WIN64
#define INSTALL_NTARCH "ntamd64"
#else
#define INSTALL_NTARCH "ntx86"
#endif

#define WFP_CALLOUT_SVC_NAME    "PiaWfpCallout"

#include "tap_inl.h"
#include "service_inl.h"

#include <set>
#include <string>

#define _SETUPAPI_VER _WIN32_WINNT_WIN7
#include <windows.h>
#include <setupapi.h>
#include <newdev.h>

#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "setupapi.lib")

static std::string utf16to8(LPCWSTR str, int len = -1)
{
    if(!str)
        return {};

    std::string result;
    result.resize(WideCharToMultiByte(CP_UTF8, 0, str, len, NULL, 0, NULL, NULL), 0);
    if (result.size())
        result.resize(WideCharToMultiByte(CP_UTF8, 0, str, len, &result[0], (int)result.size(), NULL, NULL), 0);
    return result;
}

inline std::string WinErrorEx::message() const
{
    LPWSTR errMsg{nullptr};

    auto len = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                                nullptr, code(), 0,
                                reinterpret_cast<LPWSTR>(&errMsg), 0, nullptr);
    std::string msgUtf8 = utf16to8(errMsg, len);
    ::LocalFree(errMsg);

    return msgUtf8;
}

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

static std::set<std::wstring> g_oemInfs;

class ScopedNonInteractive
{
public:
    ScopedNonInteractive(bool enable) : _enable(enable) { if (_enable) _prev = SetupSetNonInteractiveMode(TRUE); }
    ~ScopedNonInteractive() { if (_enable) SetupSetNonInteractiveMode(_prev); }
private:
    BOOL _prev;
    bool _enable;
};
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

static std::wstring getSystemRoot()
{
    std::wstring path;
    path.resize(MAX_PATH, 0);
    DWORD pathLength = GetEnvironmentVariableW(L"SYSTEMROOT", &path[0], path.size());
    if (pathLength > 0 && pathLength < path.size())
    {
        path.resize(pathLength);
        return path;
    }
    return {};
}

static bool uninstallTapDriverInf()
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

class WinInf
{
public:
    explicit WinInf(PCWSTR fileName);
    ~WinInf();

private:
    WinInf(const WinInf &) = delete;
    WinInf &operator=(const WinInf &) = delete;

public:
    HINF handle() const {return _hInf;}

private:
    HINF _hInf;
};

inline WinInf::WinInf(PCWSTR fileName)
    : _hInf{INVALID_HANDLE_VALUE}
{
    _hInf = ::SetupOpenInfFileW(fileName, nullptr, INF_STYLE_WIN4, nullptr);
    if(_hInf == INVALID_HANDLE_VALUE)
    {
        WinErrorEx error{::GetLastError()};
        TAP_LOG("Unable to open callout INF (0x%X)", error.code());
        throw error;
    }
}

inline WinInf::~WinInf()
{
    ::SetupCloseInfFile(_hInf);
}

static void traceFilePathsParam(UINT_PTR param)
{
    FILEPATHS_W *pFilePaths = reinterpret_cast<FILEPATHS_W*>(param);

    if(!pFilePaths)
    {
        TAP_LOG("File path: <null>");
    }
    else
    {
        auto source = utf16to8(pFilePaths->Source);
        auto target = utf16to8(pFilePaths->Target);
        TAP_LOG("File path: %s -> %s (0x%X, 0x%X)", source.c_str(), target.c_str(),
                pFilePaths->Win32Error, pFilePaths->Flags);
    }
}

struct CalloutInstallContext
{
    // Failed - an error was reported when copying/deleting files
    bool failed;
    // Reboot required - a file operation was delayed
    bool rebootRequired;
};

static UINT CALLBACK CalloutInstallFileCallback(void *pVoidCtx, UINT notification,
                                                UINT_PTR param1, UINT_PTR param2)
{
    CalloutInstallContext *pCtx = reinterpret_cast<CalloutInstallContext*>(pVoidCtx);

    // Copy notification flags can be OR'd together, check for these
    if(notification &
        (SPFILENOTIFY_LANGMISMATCH | SPFILENOTIFY_TARGETEXISTS | SPFILENOTIFY_TARGETNEWER))
    {
        TAP_LOG("Forcing copy of file (flags 0x%X)", notification);
        traceFilePathsParam(param1);
        return TRUE;
    }

    switch(notification)
    {
        // Initialization
        case SPFILENOTIFY_NEEDMEDIA:
            return FILEOP_DOIT;
        // Errors
        case SPFILENOTIFY_COPYERROR:
        case SPFILENOTIFY_DELETEERROR:
        case SPFILENOTIFY_RENAMEERROR:
            TAP_LOG("File operation failed (0x%X)", notification);
            traceFilePathsParam(param1);
            pCtx->failed = true;
            return FILEOP_ABORT;
        // Requires reboot
        case SPFILENOTIFY_FILEOPDELAYED:
            TAP_LOG("Reboot required due to delayed file operation");
            traceFilePathsParam(param1);
            pCtx->rebootRequired = true;
            return 0;   // Value ignored
        default:
            TAP_LOG("Unexpected notification 0x%X (0x%X, 0x%X)", notification,
                    param1, param2);
            return 0;
        // "Start" notifications - ignored
        case SPFILENOTIFY_STARTCOPY:
        case SPFILENOTIFY_STARTDELETE:
        case SPFILENOTIFY_STARTRENAME:
            return FILEOP_DOIT;
        // "End" notifications - ignored
        case SPFILENOTIFY_ENDCOPY:
        case SPFILENOTIFY_ENDDELETE:
        case SPFILENOTIFY_ENDRENAME:
            return 0;   // Value ignored
        // Unused
        case SPFILENOTIFY_STARTREGISTRATION:
        case SPFILENOTIFY_ENDREGISTRATION:
            return FILEOP_DOIT;
        // Not implemented (returns FILEOP)
        case SPFILENOTIFY_FILEINCABINET:
            TAP_LOG("Unimplemented notification 0x%X (0x%X, 0x%X)", notification,
                    param1, param2);
            return FILEOP_ABORT;
        // Not implemented (returns error)
        case SPFILENOTIFY_NEEDNEWCABINET:
        case SPFILENOTIFY_FILEEXTRACTED:
        case SPFILENOTIFY_QUEUESCAN:
        case SPFILENOTIFY_QUEUESCAN_EX:
        case SPFILENOTIFY_QUEUESCAN_SIGNERINFO:
            TAP_LOG("Unimplemented notification 0x%X (0x%X, 0x%X)", notification,
                    param1, param2);
            return ERROR_CALL_NOT_IMPLEMENTED;
        // Start queue / subqueue - ignored
        case SPFILENOTIFY_STARTQUEUE:
        case SPFILENOTIFY_STARTSUBQUEUE:
            return TRUE;
        // End queue / subqueue - ignored
        case SPFILENOTIFY_ENDQUEUE:
        case SPFILENOTIFY_ENDSUBQUEUE:
            return 0;   // Value ignored
    }
}

// Log the last error with a message
static void logLastError(const char *msg)
{
    WinErrorEx error{::GetLastError()};
    TAP_LOG("%s (0x%X): %s", msg, error.code(), error.message().c_str());
}

ServiceStatus startCalloutDriver(int timeoutMs)
{
    ServiceHandle manager{::OpenSCManagerW(nullptr, nullptr, SERVICE_START|SERVICE_QUERY_STATUS)};
    if(manager == nullptr)
    {
        logLastError("Can't open SCM");
        return ServiceStatus::ServiceStartFailed;
    }

    ServiceHandle service{::OpenServiceW(manager, L"" WFP_CALLOUT_SVC_NAME,
                                         SERVICE_START|SERVICE_QUERY_STATUS)};
    if(service == nullptr)
    {
        WinErrorEx openErr{::GetLastError()};
        if(openErr.code() == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            TAP_LOG("Callout driver is not installed");
            return ServiceStatus::ServiceNotInstalled;
        }
        else
        {
            TAP_LOG("Can't open callout driver (0x%X): %s", openErr.code(),
                    openErr.message().c_str());
            return ServiceStatus::ServiceStartFailed;
        }
    }

    return startService(service, timeoutMs);
}

static ServiceStatus stopCalloutDriver()
{
    ServiceHandle manager{::OpenSCManagerW(nullptr, nullptr, SERVICE_STOP|SERVICE_QUERY_STATUS)};
    if(manager == nullptr)
    {
        logLastError("Can't open SCM");
        return ServiceStatus::ServiceStopFailed;
    }

    ServiceHandle service{::OpenServiceW(manager, L"" WFP_CALLOUT_SVC_NAME,
                                         SERVICE_STOP | SERVICE_QUERY_STATUS)};
    if(service == nullptr)
    {
        // We opened SCM but couldn't open the service
        WinErrorEx openErr{::GetLastError()};
        if(openErr.code() == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            TAP_LOG("Service does not exist");
            return ServiceStatus::ServiceNotInstalled;
        }
        else
        {
            TAP_LOG("Can't open callout driver (0x%X): %s", openErr.code(),
                    openErr.message().c_str());
            return ServiceStatus::ServiceStopFailed;
        }
    }

    return stopService(service);
}

// Throw the last error (in installCalloutDriver()).
static void throwLastError(const char *msg)
{
    WinErrorEx error{::GetLastError()};
    TAP_LOG("%s (0x%X)", msg, error.code());
    throw error;
}

DriverStatus installCalloutDriver(LPCWSTR inf, bool nonInteractive)
{
    FUNCTION_LOGGING_CATEGORY("win.callout");
    NonInteractiveScope(nonInteractive);

    CalloutInstallContext installCtx{};

    try
    {
        WinInf calloutInf{inf};

        if(!::SetupInstallFromInfSectionW(nullptr,
                calloutInf.handle(),
                L"DefaultInstall." INSTALL_NTARCH,
                SPINST_ALL,
                nullptr,
                nullptr,
                SP_COPY_NEWER_OR_SAME,
                &CalloutInstallFileCallback,
                &installCtx,
                nullptr,
                nullptr))
        {
            throwLastError("File installation from INF failed");
        }

        if(installCtx.failed)
        {
            TAP_LOG("File installation from INF aborted due to failed file operation");
            return DriverInstallFailed;
        }
        else if(installCtx.rebootRequired)
        {
            TAP_LOG("File installation from INF succeeded, requires reboot - not starting callout now");
        }
        else
        {
            TAP_LOG("File installation from INF succeeded");
        }

        // Service installation reports "needs reboot" by setting the last error
        // code when success is returned; ensure the last error is clear.
        ::SetLastError(0);
        if(!::SetupInstallServicesFromInfSectionW(calloutInf.handle(),
                L"DefaultInstall." INSTALL_NTARCH ".Services", 0))
        {
            throwLastError("Service installation from INF failed");
        }
        else if(::GetLastError() == ERROR_SUCCESS_REBOOT_REQUIRED)
        {
            TAP_LOG("Service installation from INF succeeded, requires reboot");
            installCtx.rebootRequired = true;
        }
        else
        {
            TAP_LOG("Service installation from INF succeeded");
        }

        // Success!  The service isn't started yet, the daemon will try to start
        // it the next time we try to connect.
    }
    catch(WinErrorEx error)
    {
        TAP_LOG("Callout installation failed (0x%X): %s", error.code(),
                error.message().c_str());
        return DriverInstallFailed;
    }

    if(installCtx.rebootRequired)
        return DriverInstalledReboot;
    return DriverInstalled;
}

// Apply the last error code in uninstallCalloutDriver().  The last error is
// logged.  If error hasn't been set yet, it's set to the last error (or to a
// default code if no error code was set).
static void applyLastError(const char *msg, WinErrorEx &error)
{
    WinErrorEx newError{::GetLastError()};
    TAP_LOG("%s (0x%X)", msg, newError.code());
    if(error.code() == ERROR_SUCCESS)
    {
        if(newError.code() != ERROR_SUCCESS)
            error = newError;
        else
        {
            // It didn't return an error - set something though so we know we
            // failed.  (This has bit 29 set, so it's an "application specific
            // error").
            error = WinErrorEx{0x20000000};
        }
    }
}

DriverStatus uninstallCalloutDriver(LPCWSTR inf, bool nonInteractive)
{
    FUNCTION_LOGGING_CATEGORY("win.callout");
    NonInteractiveScope(nonInteractive);

    CalloutInstallContext installCtx{};
    bool rebootRequired = false;

    // Unlike 'install', we try to do all steps even if a prior step fails; the
    // first error is reported if more than one step fails.
    WinErrorEx error{ERROR_SUCCESS};

    try
    {
        WinInf calloutInf{inf};

        // Found the service, try to stop it
        auto stopResult = stopCalloutDriver();
        switch(stopResult)
        {
            case ServiceStatus::ServiceAlreadyStopped:
            case ServiceStatus::ServiceStopped:
                TAP_LOG("Stopped service successfully");
                break;
            case ServiceStatus::ServiceRebootNeeded:
                TAP_LOG("Service already marked for deletion, reboot required");
                rebootRequired = true;
                break;
            case ServiceStatus::ServiceNotInstalled:
                // The service doesn't seem to be installed, but go ahead and
                // continue with the uninstall in case files are still present,
                // etc.
                TAP_LOG("Service does not appear to be installed, proceeding to clean up");
                break;
            default:
                TAP_LOG("Unable to stop service - %d", stopResult);
                break;
        }

        // Remove the service
        ::SetLastError(0);
        if(!::SetupInstallServicesFromInfSectionW(calloutInf.handle(),
                L"DefaultUninstall." INSTALL_NTARCH ".Services", 0))
        {
            applyLastError("Service uninstallation from INF failed", error);
        }
        else if(::GetLastError() == ERROR_SUCCESS_REBOOT_REQUIRED)
        {
            TAP_LOG("Service uninstallation from INF succeeded, requires reboot");
            rebootRequired = true;
        }
        else
        {
            TAP_LOG("Service uninstallation from INF succeeded");
        }

        // Uninstall the driver
        if(!::SetupInstallFromInfSectionW(nullptr,
                calloutInf.handle(),
                L"DefaultUninstall." INSTALL_NTARCH,
                SPINST_ALL,
                nullptr,
                nullptr,
                SP_COPY_NEWER_OR_SAME,
                &CalloutInstallFileCallback,
                &installCtx,
                nullptr,
                nullptr))
        {
            applyLastError("File uninstallation from INF failed", error);
        }
        else if(installCtx.failed)
        {
            TAP_LOG("File uninstallation from INF aborted due to failed file operation");
        }
        else if(installCtx.rebootRequired)
        {
            TAP_LOG("File uninstallation from INF succeeded, requires reboot");
        }
        else
        {
            TAP_LOG("File uninstallation from INF succeeded");
        }

        // If the driver had been started, we usually can't do anything to
        // PiaWfpCallout.sys until the system is rebooted.  This means that the
        // uninstall above can't delete it, but it does not seem to report this.
        // Try to clean it up ourselves with a delayed delete if it still
        // exists.
        std::wstring driverInstalledPath = getSystemRoot();
        if(!driverInstalledPath.empty())
        {
            driverInstalledPath += L"\\system32\\drivers\\PiaWfpCallout.sys";
            // Try to delete the file directly first, which also tells us if the
            // file still exists
            if(::DeleteFileW(driverInstalledPath.c_str()))
            {
                TAP_LOG("Deleted driver file");
            }
            else if(::GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                TAP_LOG("Driver file was deleted by INF uninstallation");
            }
            else
            {
                WinErrorEx deleteError{::GetLastError()};
                TAP_LOG("Driver file couldn't be deleted, queue for next boot (0x%X): %s",
                        deleteError.code(), deleteError.message().c_str());
                if(!::MoveFileExW(driverInstalledPath.c_str(), nullptr,
                                    MOVEFILE_DELAY_UNTIL_REBOOT))
                {
                    applyLastError("Couldn't queue driver file removal for next boot", error);
                }
                else
                {
                    TAP_LOG("Queued driver file removal for next boot");
                    rebootRequired = true;
                }
            }
        }
    }
    catch(WinErrorEx criticalError)
    {
        // This happens if we can't open the INF at all for some reason.
        applyLastError("Callout uninstallation failed", error);
    }

    if(error.code() != ERROR_SUCCESS)
    {
        TAP_LOG("Callout uninstallation failed (0x%X): %s", error.code(),
                error.message().c_str());
        return DriverUninstallFailed;
    }
    else if(installCtx.failed)
    {
        TAP_LOG("Callout uninstallation aborted due to failed file operation");
        return DriverUninstallFailed;
    }
    else if(installCtx.rebootRequired || rebootRequired)
    {
        TAP_LOG("Callout uninstallation succeeded, requires reboot");
        return DriverUninstalledReboot;
    }

    TAP_LOG("Callout uninstallation succeeded");
    return DriverUninstalled;
}

#undef NonInteractiveScope

#endif // TAP_INL
