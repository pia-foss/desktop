#pragma once
#ifndef DCO_LOG
#define DCO_LOG(...) ((void) 0)
#endif
#ifndef FUNCTION_LOGGING_CATEGORY
#define FUNCTION_LOGGING_CATEGORY(cat) ((void) 0)
#endif

#define WIN32_LEAN_AND_MEAN
#include "common_inl.h" // DriverStatus, utf16to8 (via callout.inl in the same TU)
#include "dco_inl.h"
#include "service_inl.h"
#include "util_inl.h" // utf16ptr, appendQuotedArgument, runProgram (impl in util.inl)
#include <devguid.h>  // GUID_DEVCLASS_NET
#include <newdev.h>
#include <set>
#include <setupapi.h>
#include <string>
#include <windows.h>

#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "setupapi.lib")

// Helpers for driving tapinstall.exe.  Shared by any driver installed that way;
// DCO is currently the only one.
//
// tapinstall.exe is a devcon derivative (its help text is devcon's).  Its exit
// codes are devcon's, not Win32 error codes: 0 = success (also returned for
// help/usage, and by "remove" when no devices matched), 1 = success but a
// reboot is required, 2 = failure.  There is no distinct "nothing to
// uninstall" exit code - a remove that matches no devices succeeds with 0.
DriverStatus mapExitCodeToDriverStatus(int action, int exitCode)
{
    switch(action)
    {
        // install
        case 1:
            if(exitCode == 0)
            {
                return DriverInstalled;
            }

            if(exitCode == 1)
            {
                return DriverInstalledReboot;
            }

            return DriverInstallFailed;

        // uninstall
        case 2:
            if(exitCode == 0)
            {
                return DriverUninstalled;
            }

            if(exitCode == 1)
            {
                return DriverUninstalledReboot;
            }

            return DriverUninstallFailed;
    }

    // should never get here
    return DriverInstallFailed;
}

bool IsWindows11()
{
    typedef LONG(WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

    HMODULE hNtdll = ::GetModuleHandleW(L"ntdll.dll");
    if(!hNtdll)
    {
        return false;
    }

    auto pRtlGetVersion =
        reinterpret_cast<RtlGetVersionPtr>(::GetProcAddress(hNtdll, "RtlGetVersion"));
    if(!pRtlGetVersion)
    {
        return false;
    }

    RTL_OSVERSIONINFOW vi{};
    vi.dwOSVersionInfoSize = sizeof(vi);

    if(pRtlGetVersion(&vi) != 0)
    {
        return false;
    }

    // Windows 11 starts at build 22000.
    return (vi.dwMajorVersion == 10 && vi.dwMinorVersion == 0 && vi.dwBuildNumber >= 22000);
}

bool dcoDeviceInstalled()
{
    FUNCTION_LOGGING_CATEGORY("win.dco");

    HDEVINFO devInfoSet =
        SetupDiGetClassDevsExW(&GUID_DEVCLASS_NET, NULL, NULL, 0, NULL, NULL, NULL);
    if(devInfoSet == INVALID_HANDLE_VALUE)
    {
        DCO_LOG("Unable to enumerate network devices: %d", (int) ::GetLastError());
        return false;
    }

    bool found = false;
    SP_DEVINFO_DATA devInfoData{};
    devInfoData.cbSize = sizeof(devInfoData);
    for(DWORD i = 0; !found && ::SetupDiEnumDeviceInfo(devInfoSet, i, &devInfoData); ++i)
    {
        // Hardware IDs are a REG_MULTI_SZ list; the device matches if any of
        // them is ours.  (Hardware IDs are not case sensitive.)
        wchar_t hardwareIds[512]{};
        DWORD dataType = 0, actualSize = 0;
        if(!::SetupDiGetDeviceRegistryPropertyW(devInfoSet, &devInfoData, SPDRP_HARDWAREID,
                                                &dataType, reinterpret_cast<BYTE *>(hardwareIds),
                                                sizeof(hardwareIds) - sizeof(wchar_t),
                                                &actualSize))
        {
            continue;
        }
        if(dataType != REG_MULTI_SZ)
        {
            continue;
        }

        for(const wchar_t *pId = hardwareIds; *pId; pId += wcslen(pId) + 1)
        {
            if(!_wcsicmp(pId, DCO_DEVICE_NAME))
            {
                found = true;
                break;
            }
        }
    }

    ::SetupDiDestroyDeviceInfoList(devInfoSet);
    DCO_LOG("Dco device installed: %d", (int) found);
    return found;
}

DriverStatus installDcoDriver(LPCWSTR inf, LPCWSTR deviceName, LPCWSTR tapInstallPath)
{
    FUNCTION_LOGGING_CATEGORY("win.dco");
    DCO_LOG("Installing DCO driver from INF: %s", utf16to8(inf).c_str());
    DCO_LOG("Device name: %s", utf16to8(deviceName).c_str());

    DCO_LOG("Running tapinstall.exe with arguments: install %s, %s", utf16to8(inf).c_str(),
            utf16to8(deviceName).c_str());

    int exitCode = runProgram(tapInstallPath, {L"install", inf, deviceName});
    DCO_LOG("tapinstall.exe exited with code: %d", exitCode);
    DriverStatus tapInstallResult = mapExitCodeToDriverStatus(1, exitCode);

    if(tapInstallResult != DriverInstalled && tapInstallResult != DriverInstalledReboot)
    {
        DCO_LOG("Driver install failed with tapinstall.exe: %d", tapInstallResult);
    }

    return tapInstallResult;
}

DriverStatus uninstallDcoDriver(LPCWSTR inf, LPCWSTR deviceName, LPCWSTR tapInstallPath)
{
    FUNCTION_LOGGING_CATEGORY("win.dco");

    int tapInstallExitCode = runProgram(tapInstallPath, {L"remove", deviceName});
    DriverStatus tapInstallResult = mapExitCodeToDriverStatus(2, tapInstallExitCode);

    DCO_LOG("tapinstall.exe exited with code: %d", tapInstallExitCode);

    if(tapInstallResult != DriverUninstalled && tapInstallResult != DriverUninstalledReboot)
    {
        DCO_LOG("Driver uninstall failed with tapinstall.exe: %d", tapInstallResult);
    }

    return tapInstallResult;
}
