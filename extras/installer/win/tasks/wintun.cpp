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

#include "wintun.h"
#include "brand.h"
#include "util.h"
#include "service_inl.h"
#include "safemode_inl.h"
#include "tap_inl.h"
#include "tun_inl.h"
#include <Msi.h>
#include <MsiQuery.h>
#include <string>
#include <algorithm>
#include <cassert>
#include <array>

#pragma comment(lib, "Msi.lib")

namespace
{
    std::wstring getWintunMsiPath()
    {
        return g_installPath + L"\\wintun\\" BRAND_CODE "-wintun.msi";
    }

    // Constants used to handle queued WinTUN uninstallation in safe mode
    const HKEY runKeyRoot = HKEY_LOCAL_MACHINE;
    const wchar_t *runOnceKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
    const wchar_t *piaUninstallValueName{L"" BRAND_NAME " WinTUN Uninstall"};

    const HKEY uninstallKeyRoot = HKEY_LOCAL_MACHINE;
    const wchar_t *uninstallKeyPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

    std::wstring getRegValue(HKEY key, const wchar_t *subkey,
                             const wchar_t *valueName, DWORD typeFlags)
    {
        DWORD valueSize{0};
        WinErrorEx err = RegGetValueW(key, subkey, valueName, typeFlags,
                                      nullptr, nullptr, &valueSize);
        if(err.code() != ERROR_SUCCESS)
        {
            LOG("Can't get value from key %ls, unable to get length of %ls - 0x%X: %s",
                subkey, valueName, err.code(), err.message().c_str());
            return {};
        }

        std::wstring value;
        value.resize(valueSize/sizeof(wchar_t));
        err = RegGetValueW(key, subkey, valueName, typeFlags, nullptr,
                           value.data(), &valueSize);
        if(err.code() != ERROR_SUCCESS)
        {
            LOG("Can't get value from key %ls, unable to get %ls - 0x%X: %s",
                subkey, valueName, err.code(), err.message().c_str());
            return {};
        }

        // Remove the trailing null char
        value.resize((valueSize/sizeof(wchar_t))-1);
        return value;
    }

    bool isPiaWintunUninstallEntry(HKEY uninstallKey, wchar_t *entryName)
    {
        std::wstring dispName = getRegValue(uninstallKey, entryName,
                                            L"DisplayName", RRF_RT_REG_SZ);
        LOG("Entry %ls -> disp name %ls (%u)", entryName, dispName.c_str(), dispName.size());
        return dispName == L"" BRAND_WINTUN_PRODUCT_NAME;
    }

    std::wstring findPIAWintunProduct(HKEY uninstallKey)
    {
        // It's possible to find the length of the longest subkey name with
        // RegQueryInfoKeyW(), but key names are limited to 255 chars anyway -
        // https://docs.microsoft.com/en-us/windows/win32/sysinfo/registry-element-size-limits
        // It's not clear whether that's intended to include a null terminator
        // though, so use 256 to be on the safe side.
        std::array<wchar_t, 256> subkeyNameBuf{};
        for(DWORD i=0; i<std::numeric_limits<DWORD>::max(); ++i)
        {
            DWORD nameLen = subkeyNameBuf.size();
            WinErrorEx enumErr = ::RegEnumKeyExW(uninstallKey, i,
                                                 subkeyNameBuf.data(),
                                                 &nameLen, nullptr, nullptr,
                                                 nullptr, nullptr);
            switch(enumErr.code())
            {
            case ERROR_MORE_DATA:
                // Unlikely due to name size limits, but if it does happen just
                // skip it, the PIA key we're looking for is expected to have a
                // GUID name anyway.
                LOG("Skipping uninstall subkey %u, name exceeds limit of %u",
                    i, subkeyNameBuf.size());
                break;
            case ERROR_NO_MORE_ITEMS:
                LOG("Unable to find uninstall data for WinTUN after enumerating %u entries",
                    i+1);
                return {};
            default:
                LOG("Unable to find uninstall data for WinTUN, enumeration failed after %u items with code 0x%X: %s",
                    i, enumErr.code(), enumErr.message().c_str());
                return {};
            case ERROR_SUCCESS:
                // If it's our WinTUN driver's entry, return the product ID (the
                // key name)
                if(isPiaWintunUninstallEntry(uninstallKey, subkeyNameBuf.data()))
                    return std::wstring{subkeyNameBuf.data(), nameLen};
                break;
            }
        }

        return {};
    }
}

void UninstallWintunTask::execute()
{
    if(getBootMode() != BootMode::Normal)
    {
        // msiservice can't be started in Safe Mode.  Don't even check the
        // installed product version - we get some bogus version number in this
        // state.
        //
        // Queue up an msiexec command to uninstall WinTUN the next time the
        // system is booted normally.  If PIA is reinstalled before this
        // happens, it'll delete this registry entry so it doesn't interfere
        // with the new installation.
        LOG("Queue WinTUN uninstall for next boot, can't uninstall in safe mode");

        // Find the product in the uninstall area of the registry.
        //
        // msiexec /x requires a product code, not an upgrade code, and we don't
        // have the product code at this point (it's generated by WiX during the
        // MSI build).  Normally we'd use findInstalledWintunProducts() to find
        // it, but the MSI service isn't running.  Instead, find our WinTUN
        // driver's uninstall entry, which contains the product code.
        HKEY uninstallKey{nullptr};
        WinErrorEx err = RegOpenKeyExW(uninstallKeyRoot, uninstallKeyPath, 0,
                                       KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE,
                                       &uninstallKey);
        if(err.code() != ERROR_SUCCESS || !uninstallKey)
        {
            LOG("Can't remove WinTUN, unable to find uninstall entry - 0x%X: %s",
                err.code(), err.message().c_str());
            return;
        }

        std::wstring product = findPIAWintunProduct(uninstallKey);
        if(!product.empty())
        {
            LOG("Scheduling WinTUN removal with for product %ls", product.c_str());
            // The registry key does include an UninstallCmd, which is
            // "msiexec /x<product_id>", but we also want /qn to do it silently.
            std::wstring uninstallCmd{L"msiexec.exe /qn /x"};
            uninstallCmd += product;
            // Set a RunOnce key to uninstall this product the next time the
            // system is booted normally.  (size+1)*sizeof(wchar_t) is required
            // because the size is in bytes and includes the null char.
            err = RegSetKeyValueW(runKeyRoot, runOnceKeyPath,
                                  piaUninstallValueName, REG_SZ,
                                  uninstallCmd.data(),
                                  (uninstallCmd.size()+1)*sizeof(wchar_t));
            if(err.code() != ERROR_SUCCESS)
            {
                LOG("Can't schedule WinTUN removal for product %ls - 0x%X, %s",
                    product.c_str(), err.code(), err.message().c_str());
            }
        }

        return;
    }

    auto installedProducts = findInstalledWintunProducts();

    if(installedProducts.empty())
    {
        LOG("WinTUN package is not installed, nothing to uninstall");
        return;
    }

    // Uninstall the package.  Retry as long as the user selects retry after a
    // failure.
    // In principle, there could be more than one installed package, though it
    // should not really happen:
    // - if all uninstalls succeed or are ignored, reinstall on rollback
    // - if any uninstall fails, prompt and retry as long as the user says to
    //   retry
    //
    // For one installed product, this reduces to reinstalling on rollback if
    // the uninstall succeeds or is ignored.
    auto itInstalledProduct = installedProducts.begin();
    while(itInstalledProduct != installedProducts.end())
    {
        if(uninstallMsiProduct(itInstalledProduct->c_str()))
        {
            LOG("Uninstalled product %S", *itInstalledProduct);
            ++itInstalledProduct;
        }
        else if(InstallerError::raise(Abort | Retry | Ignore, IDS_MB_WINTUNUNINSTALLFAILED) == Ignore)
        {
            LOG("User ignored WinTUN uninstall failure for product %S",
                *itInstalledProduct);
            ++itInstalledProduct;
        }
        // Otherwise, the user selected "retry" to retry uninstalling this
        // product ("abort" throws an exception).
    }

    LOG("Finished uninstalling products for WinTUN package");
}

void UninstallWintunTask::rollback()
{
    LOG("Reinstalling WinTUN package for rollback");
    if(::installMsiPackage(getWintunMsiPath().c_str()))
        LOG("Rollback installation succeeded");
    else
        LOG("Rollback installation failed");
}

void InstallWintunTask::execute()
{
    if(::IsWindows8OrGreater())
    {
        if(!writeTextFile(g_daemonDataPath + L"\\.need-wintun-install", "", CREATE_ALWAYS))
            LOG("Unable to flag WinTUN installation needed");
        _rollbackRunOnceRestore = getRegValue(runKeyRoot, runOnceKeyPath,
                                  piaUninstallValueName, RRF_RT_REG_SZ);
        RegDeleteKeyValueW(runKeyRoot, runOnceKeyPath, piaUninstallValueName);
    }
    else
        LOG("Skipping WinTUN installation - not supported on this OS version");
}

void InstallWintunTask::rollback()
{
    ::DeleteFileW((g_daemonDataPath + L"\\.need-wintun-install").c_str());
    if(!_rollbackRunOnceRestore.empty())
    {
        RegSetKeyValueW(runKeyRoot, runOnceKeyPath, piaUninstallValueName,
                        REG_SZ, _rollbackRunOnceRestore.data(),
                        (_rollbackRunOnceRestore.size()+1)*sizeof(wchar_t));
    }
}
