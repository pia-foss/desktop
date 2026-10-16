#pragma once

#ifndef CALLOUT_LOG
#define CALLOUT_LOG(...) ((void) 0)
#endif
#ifndef FUNCTION_LOGGING_CATEGORY
#define FUNCTION_LOGGING_CATEGORY(cat) ((void) 0)
#endif

#if defined(_M_ARM64)
#define INSTALL_NTARCH "ntarm64"
#elif defined(_WIN64)
#define INSTALL_NTARCH "ntamd64"
#else
#error Unsupported architecture
#endif

#define WFP_CALLOUT_SVC_NAME    "PiaWfpCallout"

#include "callout_inl.h"
#include "service_inl.h"

#include <set>
#include <string>

#define _SETUPAPI_VER _WIN32_WINNT_WIN7
#include <newdev.h>
#include <setupapi.h>
#include <windows.h>

#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "setupapi.lib")

static std::string utf16to8(LPCWSTR str, int len = -1)
{
    if(!str)
    {
        return {};
    }

    std::string result;
    result.resize(WideCharToMultiByte(CP_UTF8, 0, str, len, NULL, 0, NULL, NULL), 0);
    if(result.size())
    {
        result.resize(
            WideCharToMultiByte(CP_UTF8, 0, str, len, &result[0], (int) result.size(), NULL, NULL),
            0);
    }
    return result;
}

inline std::string WinErrorEx::message() const
{
    LPWSTR errMsg{nullptr};

    auto len = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                                nullptr, code(), 0, reinterpret_cast<LPWSTR>(&errMsg), 0, nullptr);
    std::string msgUtf8 = utf16to8(errMsg, len);
    ::LocalFree(errMsg);

    return msgUtf8;
}

class ScopedNonInteractive
{
public:
    ScopedNonInteractive(bool enable)
        : _enable(enable)
    {
        if(_enable)
        {
            _prev = SetupSetNonInteractiveMode(TRUE);
        }
    }
    ~ScopedNonInteractive()
    {
        if(_enable)
        {
            SetupSetNonInteractiveMode(_prev);
        }
    }

private:
    BOOL _prev;
    bool _enable;
};
#define NonInteractiveScope(enable)                                                                \
    ScopedNonInteractive nonInteractiveBlock(nonInteractive);                                      \
    (void) nonInteractiveBlock

static std::wstring getSystemRoot()
{
    std::wstring path;
    path.resize(MAX_PATH, 0);
    DWORD pathLength = GetEnvironmentVariableW(L"SYSTEMROOT", &path[0], path.size());
    if(pathLength > 0 && pathLength < path.size())
    {
        path.resize(pathLength);
        return path;
    }
    return {};
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
    HINF handle() const
    {
        return _hInf;
    }

private:
    HINF _hInf;
};

inline WinInf::WinInf(PCWSTR fileName)
    : _hInf{::SetupOpenInfFileW(fileName, nullptr, INF_STYLE_WIN4, nullptr)}
{
    if(_hInf == INVALID_HANDLE_VALUE)
    {
        WinErrorEx error{::GetLastError()};
        CALLOUT_LOG("Unable to open callout INF (0x%lX)", error.code());
        throw error;
    }
}

inline WinInf::~WinInf()
{
    ::SetupCloseInfFile(_hInf);
}

static void traceFilePathsParam(UINT_PTR param)
{
    FILEPATHS_W *pFilePaths = reinterpret_cast<FILEPATHS_W *>(param);

    if(!pFilePaths)
    {
        CALLOUT_LOG("File path: <null>");
    }
    else
    {
        auto source = utf16to8(pFilePaths->Source);
        auto target = utf16to8(pFilePaths->Target);
        CALLOUT_LOG("File path: %s -> %s (0x%lX, 0x%lX)", source.c_str(), target.c_str(),
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

static UINT CALLBACK CalloutInstallFileCallback(void *pVoidCtx, UINT notification, UINT_PTR param1,
                                                UINT_PTR param2)
{
    CalloutInstallContext *pCtx = reinterpret_cast<CalloutInstallContext *>(pVoidCtx);

    // Copy notification flags can be OR'd together, check for these
    if(notification &
       (SPFILENOTIFY_LANGMISMATCH | SPFILENOTIFY_TARGETEXISTS | SPFILENOTIFY_TARGETNEWER))
    {
        CALLOUT_LOG("Forcing copy of file (flags 0x%X)", notification);
        traceFilePathsParam(param1);
        return TRUE;
    }

    switch(notification)
    {
        // Initialization
        case SPFILENOTIFY_NEEDMEDIA: return FILEOP_DOIT;
        // Errors
        case SPFILENOTIFY_COPYERROR:
        case SPFILENOTIFY_DELETEERROR:
        case SPFILENOTIFY_RENAMEERROR:
            CALLOUT_LOG("File operation failed (0x%X)", notification);
            traceFilePathsParam(param1);
            pCtx->failed = true;
            return FILEOP_ABORT;
        // Requires reboot
        case SPFILENOTIFY_FILEOPDELAYED:
            CALLOUT_LOG("Reboot required due to delayed file operation");
            traceFilePathsParam(param1);
            pCtx->rebootRequired = true;
            return 0; // Value ignored
        default:
            CALLOUT_LOG("Unexpected notification 0x%X (0x%llX, 0x%llX)", notification, param1,
                        param2);
            return 0;
        // "Start" notifications - ignored
        case SPFILENOTIFY_STARTCOPY:
        case SPFILENOTIFY_STARTDELETE:
        case SPFILENOTIFY_STARTRENAME: return FILEOP_DOIT;
        // "End" notifications - ignored
        case SPFILENOTIFY_ENDCOPY:
        case SPFILENOTIFY_ENDDELETE:
        case SPFILENOTIFY_ENDRENAME:
            return 0; // Value ignored
        // Unused
        case SPFILENOTIFY_STARTREGISTRATION:
        case SPFILENOTIFY_ENDREGISTRATION: return FILEOP_DOIT;
        // Not implemented (returns FILEOP)
        case SPFILENOTIFY_FILEINCABINET:
            CALLOUT_LOG("Unimplemented notification 0x%X (0x%llX, 0x%llX)", notification, param1,
                        param2);
            return FILEOP_ABORT;
        // Not implemented (returns error)
        case SPFILENOTIFY_NEEDNEWCABINET:
        case SPFILENOTIFY_FILEEXTRACTED:
        case SPFILENOTIFY_QUEUESCAN:
        case SPFILENOTIFY_QUEUESCAN_EX:
        case SPFILENOTIFY_QUEUESCAN_SIGNERINFO:
            CALLOUT_LOG("Unimplemented notification 0x%X (0x%llX, 0x%llX)", notification, param1,
                        param2);
            return ERROR_CALL_NOT_IMPLEMENTED;
        // Start queue / subqueue - ignored
        case SPFILENOTIFY_STARTQUEUE:
        case SPFILENOTIFY_STARTSUBQUEUE: return TRUE;
        // End queue / subqueue - ignored
        case SPFILENOTIFY_ENDQUEUE:
        case SPFILENOTIFY_ENDSUBQUEUE: return 0; // Value ignored
    }
}

// Log the last error with a message
static void logLastError(const char *msg)
{
    WinErrorEx error{::GetLastError()};
    CALLOUT_LOG("%s (0x%lX): %s", msg, error.code(), error.message().c_str());
}

ServiceStatus startCalloutDriver(int timeoutMs)
{
    ServiceHandle manager{::OpenSCManagerW(nullptr, nullptr, SERVICE_START | SERVICE_QUERY_STATUS)};
    if(manager == nullptr)
    {
        logLastError("Can't open SCM");
        return ServiceStatus::ServiceStartFailed;
    }

    ServiceHandle service{::OpenServiceW(manager, L"" WFP_CALLOUT_SVC_NAME,
                                         SERVICE_START | SERVICE_QUERY_STATUS)};
    if(service == nullptr)
    {
        WinErrorEx openErr{::GetLastError()};
        if(openErr.code() == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            CALLOUT_LOG("Callout driver is not installed");
            return ServiceStatus::ServiceNotInstalled;
        }
        else
        {
            CALLOUT_LOG("Can't open callout driver (0x%lX): %s", openErr.code(),
                        openErr.message().c_str());
            return ServiceStatus::ServiceStartFailed;
        }
    }

    return startService(service, timeoutMs);
}

ServiceStatus stopCalloutDriver()
{
    ServiceHandle manager{::OpenSCManagerW(nullptr, nullptr, SERVICE_STOP | SERVICE_QUERY_STATUS)};
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
            CALLOUT_LOG("Service does not exist");
            return ServiceStatus::ServiceNotInstalled;
        }
        else
        {
            CALLOUT_LOG("Can't open callout driver (0x%lX): %s", openErr.code(),
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
    CALLOUT_LOG("%s (0x%lX)", msg, error.code());
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

        if(!::SetupInstallFromInfSectionW(
               nullptr, calloutInf.handle(), L"DefaultInstall." INSTALL_NTARCH, SPINST_ALL, nullptr,
               nullptr, SP_COPY_NEWER_OR_SAME, &CalloutInstallFileCallback, &installCtx, nullptr,
               nullptr))
        {
            throwLastError("File installation from INF failed");
        }

        if(installCtx.failed)
        {
            CALLOUT_LOG("File installation from INF aborted due to failed file operation");
            return DriverInstallFailed;
        }
        else if(installCtx.rebootRequired)
        {
            CALLOUT_LOG(
                "File installation from INF succeeded, requires reboot - not starting callout now");
        }
        else
        {
            CALLOUT_LOG("File installation from INF succeeded");
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
            CALLOUT_LOG("Service installation from INF succeeded, requires reboot");
            installCtx.rebootRequired = true;
        }
        else
        {
            CALLOUT_LOG("Service installation from INF succeeded");
        }

        // Success!  The service isn't started yet, the daemon will try to start
        // it the next time we try to connect.
    }
    catch(WinErrorEx error)
    {
        CALLOUT_LOG("Callout installation failed (0x%lX): %s", error.code(),
                    error.message().c_str());
        return DriverInstallFailed;
    }

    if(installCtx.rebootRequired)
    {
        return DriverInstalledReboot;
    }
    return DriverInstalled;
}

// Apply the last error code in uninstallCalloutDriver().  The last error is
// logged.  If error hasn't been set yet, it's set to the last error (or to a
// default code if no error code was set).
static void applyLastError(const char *msg, WinErrorEx &error)
{
    WinErrorEx newError{::GetLastError()};
    CALLOUT_LOG("%s (0x%lX)", msg, newError.code());
    if(error.code() == ERROR_SUCCESS)
    {
        if(newError.code() != ERROR_SUCCESS)
        {
            error = newError;
        }
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
            case ServiceStatus::ServiceStopped: CALLOUT_LOG("Stopped service successfully"); break;
            case ServiceStatus::ServiceRebootNeeded:
                CALLOUT_LOG("Service already marked for deletion, reboot required");
                rebootRequired = true;
                break;
            case ServiceStatus::ServiceNotInstalled:
                // The service doesn't seem to be installed, but go ahead and
                // continue with the uninstall in case files are still present,
                // etc.
                CALLOUT_LOG("Service does not appear to be installed, proceeding to clean up");
                break;
            default: CALLOUT_LOG("Unable to stop service - %d", stopResult); break;
        }

        // Remove the service
        ::SetLastError(0);
        if(!::SetupInstallServicesFromInfSectionW(
               calloutInf.handle(), L"DefaultUninstall." INSTALL_NTARCH ".Services", 0))
        {
            applyLastError("Service uninstallation from INF failed", error);
        }
        else if(::GetLastError() == ERROR_SUCCESS_REBOOT_REQUIRED)
        {
            CALLOUT_LOG("Service uninstallation from INF succeeded, requires reboot");
            rebootRequired = true;
        }
        else
        {
            CALLOUT_LOG("Service uninstallation from INF succeeded");
        }

        // Uninstall the driver
        if(!::SetupInstallFromInfSectionW(
               nullptr, calloutInf.handle(), L"DefaultUninstall." INSTALL_NTARCH, SPINST_ALL,
               nullptr, nullptr, SP_COPY_NEWER_OR_SAME, &CalloutInstallFileCallback, &installCtx,
               nullptr, nullptr))
        {
            applyLastError("File uninstallation from INF failed", error);
        }
        else if(installCtx.failed)
        {
            CALLOUT_LOG("File uninstallation from INF aborted due to failed file operation");
        }
        else if(installCtx.rebootRequired)
        {
            CALLOUT_LOG("File uninstallation from INF succeeded, requires reboot");
        }
        else
        {
            CALLOUT_LOG("File uninstallation from INF succeeded");
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
                CALLOUT_LOG("Deleted driver file");
            }
            else if(::GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                CALLOUT_LOG("Driver file was deleted by INF uninstallation");
            }
            else
            {
                WinErrorEx deleteError{::GetLastError()};
                CALLOUT_LOG("Driver file couldn't be deleted, queue for next boot (0x%lX): %s",
                            deleteError.code(), deleteError.message().c_str());
                if(!::MoveFileExW(driverInstalledPath.c_str(), nullptr,
                                  MOVEFILE_DELAY_UNTIL_REBOOT))
                {
                    applyLastError("Couldn't queue driver file removal for next boot", error);
                }
                else
                {
                    CALLOUT_LOG("Queued driver file removal for next boot");
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
        CALLOUT_LOG("Callout uninstallation failed (0x%lX): %s", error.code(),
                    error.message().c_str());
        return DriverUninstallFailed;
    }
    else if(installCtx.failed)
    {
        CALLOUT_LOG("Callout uninstallation aborted due to failed file operation");
        return DriverUninstallFailed;
    }
    else if(installCtx.rebootRequired || rebootRequired)
    {
        CALLOUT_LOG("Callout uninstallation succeeded, requires reboot");
        return DriverUninstalledReboot;
    }

    CALLOUT_LOG("Callout uninstallation succeeded");
    return DriverUninstalled;
}
#undef NonInteractiveScope
