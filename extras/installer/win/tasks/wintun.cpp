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

#include "wintun.h"
#include "brand.h"
#include "util.h"
#include "service_inl.h"
#include "tap_inl.h"
#include "tun_inl.h"
#include <Msi.h>
#include <MsiQuery.h>
#include <string>
#include <algorithm>
#include <cassert>

#pragma comment(lib, "Msi.lib")

namespace
{
    // The shipped version of the WinTUN driver
    const WinDriverVersion shippedWintunVersion{1, 0, 0, 0};

    std::wstring getWintunMsiPath()
    {
        return g_installPath + L"\\wintun\\" BRAND_CODE "-wintun.msi";
    }

    class MsiHandle
    {
    public:
        // Open a package handle.  If this fails, the error is traced, and the
        // MsiHandle holds a null handle.  See MsiOpenPackageExW().
        MsiHandle(const wchar_t *pPkgPath, DWORD options)
            : _handle{}
        {
            UINT pkgErr = ::MsiOpenPackageExW(pPkgPath, options, &_handle);
            if(pkgErr)
            {
                LOG("Unable to open MSI package %S - %u", pPkgPath, pkgErr);
                _handle = 0;
                return;
            }
        }

        ~MsiHandle()
        {
            if(_handle)
                ::MsiCloseHandle(_handle);
        }

    private:
        MsiHandle(const MsiHandle &) = delete;
        MsiHandle &operator=(const MsiHandle &) = delete;

    public:
        explicit operator bool() const {return _handle;}
        operator MSIHANDLE() const {return _handle;}
        MSIHANDLE get() const {return _handle;}

    private:
        MSIHANDLE _handle;
    };

    // Get a property of an installed MSI product - returns "" if the version
    // can't be retrieved.
    std::wstring getProductProperty(const wchar_t *pProductCode,
                                    const wchar_t *pProperty)
    {
        DWORD valueLen{0};
        unsigned queryResult = ::MsiGetProductInfoExW(pProductCode, nullptr,
                                                      MSIINSTALLCONTEXT_MACHINE,
                                                      pProperty, nullptr,
                                                      &valueLen);
        if(queryResult != ERROR_SUCCESS)
        {
            LOG("Can't get length of %S, result %u - %S", pProperty,
                queryResult, pProductCode);
            return {};
        }

        std::wstring value;
        // Add room for the null terminator
        ++valueLen;
        value.resize(valueLen);
        queryResult = ::MsiGetProductInfoExW(pProductCode, nullptr,
                                             MSIINSTALLCONTEXT_MACHINE,
                                             pProperty, value.data(),
                                             &valueLen);
        if(queryResult != ERROR_SUCCESS)
        {
            LOG("Can't get %S, result %u - %S", pProperty, queryResult,
                pProductCode);
            return {};
        }
        value.resize(valueLen);
        return value;
    }

    std::wstring getMsiHandleProperty(MSIHANDLE handle, const wchar_t *pProperty)
    {
        // This is different from MsiGetProductInfoExW(), we have to pass a
        // valid string and buffer length 0 to get the length of the value (not
        // a null buffer).
        wchar_t dummy[]{L"\0"}; // Mutable empty string
        DWORD valueLen{0};
        unsigned queryResult = ::MsiGetPropertyW(handle, pProperty, dummy,
                                                 &valueLen);
        if(queryResult == ERROR_SUCCESS)
        {
            // The property is empty (it fit in a 0-length buffer)
            return {};
        }

        if(queryResult != ERROR_MORE_DATA)
        {
            LOG("Can't get length of %S, result %u", pProperty, queryResult);
            return {};
        }

        // Add room for the null terminator
        ++valueLen;
        std::wstring value;
        value.resize(valueLen);
        queryResult = ::MsiGetPropertyW(handle, pProperty, value.data(),
                                        &valueLen);
        if(queryResult != ERROR_SUCCESS)
        {
            LOG("Can't get %S, result %u", pProperty, queryResult);
            return {};
        }
        value.resize(valueLen);
        return value;
    }

    // Parse a product version string.  Reads three version parts only, the
    // fourth is set to 0.  Returns 0.0.0 if the version is not valid.
    WinDriverVersion parseProductVersion(const std::wstring &value)
    {
        enum { PartCount = 3 };
        WORD versionParts[PartCount];
        int i=0;
        const wchar_t *pNext = value.c_str();
        while(i < PartCount && pNext && *pNext)
        {
            wchar_t *pPartEnd{nullptr};
            unsigned long part = std::wcstoul(pNext, &pPartEnd, 10);
            // Not valid if:
            // - no characters consumed (not pointing to a digit)
            // - version part exceeds 65535 (MSI limit)
            // - not pointing to a '.' or '\0' following the part
            if(pPartEnd == pNext || part > 65535 || !pPartEnd ||
               (*pPartEnd != '.' && *pPartEnd != '\0'))
            {
                LOG("Product version is not valid - %S", value);
                return {};
            }
            versionParts[i] = static_cast<WORD>(part);
            // If we hit the end of the string, we're done - tolerate versions
            // with fewer than 3 parts
            if(!*pPartEnd)
                break;
            // Otherwise, continue with the next part.  (pPartEnd+1) is valid
            // because the string is null-terminated and it's currently pointing
            // to a '.'.
            pNext = pPartEnd+1;
            ++i;
        }
        LOG("Product version %u.%u.%u is installed (%S)",
            versionParts[0], versionParts[1], versionParts[2], value);
        return {versionParts[0], versionParts[1], versionParts[2], 0};
    }
}

bool WintunTask::_loadedProducts{false};
std::vector<std::wstring> WintunTask::_installedProducts;
WinDriverVersion WintunTask::_installedVersion;
bool WintunTask::_rollbackReinstall{false};

void WintunTask::loadProducts()
{
    if(_loadedProducts)
        return;

    _installedProducts = findInstalledWintunProducts();

    // If, somehow, more than one product with our upgrade code is installed,
    // uninstall them all (assume they are newer than the shipped package).
    // Shouldn't happen since the upgrade settings in the MSI should prevent
    // this.
    if(_installedProducts.size() > 1)
    {
        LOG("Found %u installed products, expected 0-1", _installedProducts.size());
        _installedVersion = WinDriverVersion{65535, 0, 0, 0};
    }
    else if(_installedProducts.size() == 1)
    {
        auto versionStr = getProductProperty(_installedProducts[0].c_str(),
                                             INSTALLPROPERTY_VERSIONSTRING);
        _installedVersion = parseProductVersion(versionStr);
    }
    // Otherwise, nothing is installed, leave _installedVersion set to 0.

    _loadedProducts = true;
}

const WinDriverVersion &WintunTask::getInstalledVersion()
{
    assert(_loadedProducts);
    return _installedVersion;
}

const std::vector<std::wstring> &WintunTask::getInstalledProducts()
{
    assert(_loadedProducts);
    return _installedProducts;
}

bool WintunTask::uninstallProduct(const wchar_t *pProductCode)
{
    return ::uninstallMsiProduct(pProductCode);
}

bool WintunTask::installPackage()
{
    return ::installMsiPackage(getWintunMsiPath().c_str());
}

void UninstallWintunTask::execute()
{
    loadProducts();

    if(getInstalledVersion() == WinDriverVersion{})
    {
        LOG("WinTUN package is not installed, nothing to uninstall");
        return;
    }

    // In the installer, only uninstall the driver if it is newer than the
    // shipped version (applies for a PIA downgrade).
    //
    // The MSI package is flagged to prevent downgrades, so users don't
    // accidentally downgrade it if driver packages are sent out by support,
    // etc.  However, if PIA itself is downgraded, we do want to downgrade the
    // driver package.
#ifdef INSTALLER
    if(getInstalledVersion() == shippedWintunVersion)
    {
        LOG("Version %s of WinTUN package is installed, do not need to install version %s",
            getInstalledVersion().printable(), shippedWintunVersion.printable());
        return;
    }
    if(getInstalledVersion() < shippedWintunVersion)
    {
        LOG("Version %s of WinTUN package is installed, do not need to uninstall before installing verison %s",
            getInstalledVersion().printable(), shippedWintunVersion.printable());
        return;
    }
#endif

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
    const auto &installedProducts = getInstalledProducts();
    auto itInstalledProduct = installedProducts.begin();
    while(itInstalledProduct != installedProducts.end())
    {
        if(uninstallProduct(itInstalledProduct->c_str()))
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

    LOG("Finished uninstalling products for version %s", getInstalledVersion().printable());
}

void UninstallWintunTask::rollback()
{
    if(_rollbackReinstall)
    {
        LOG("Reinstalling WinTUN package for rollback");
        installPackage();
    }
}

void InstallWintunTask::execute()
{
    // Skip this task on Win 7.
    if(!isWintunSupported())
    {
        LOG("Skipping WinTUN tasks, not supported on this OS");
        return;
    }

    // If shipped version is the same as the installed version, there's nothing
    // to do.
    if(getInstalledVersion() == shippedWintunVersion)
    {
        LOG("WinTUN package %s is already installed, nothing to do",
            getInstalledVersion().printable());
        return;
    }

    // Install the shipped package.   If the installed version was newer,
    // UninstallWintunTask already uninstalled it.  Otherwise, it was older, and
    // we'll install over it.
    LOG("Installing WinTUN - package %s -> %s", getInstalledVersion().printable(),
        shippedWintunVersion.printable());

    // Loop for failures if the user selects "retry"
    while(true)
    {
        if(installPackage())
        {
            // Success, mark rollback actions
            _rollbackUninstall = true;
            // _rollbackReinstall might already be set if UninstallWintunTask
            // had to uninstall a newer version, but if the existing version was
            // older, it's not set until we replace that version with the new
            // install.
            if(getInstalledVersion() != WinDriverVersion{})
                _rollbackReinstall = true;
            break;
        }

        if(InstallerError::raise(Abort | Retry | Ignore, IDS_MB_WINTUNINSTALLFAILED) == Ignore)
        {
            LOG("User ignored WinTUN install failure");
            return;
        }
    }
}

void InstallWintunTask::rollback()
{
    if(_rollbackUninstall)
    {
        // Uninstall the shipped package by finding its product code
        std::wstring shippedProduct;
        {
            MsiHandle shippedPkg{getWintunMsiPath().c_str(), 0};
            if(shippedPkg)
                shippedProduct = getMsiHandleProperty(shippedPkg, L"ProductCode");
        }

        if(shippedProduct.empty())
        {
            LOG("Can't uninstall WinTUN package for rollback, couldn't get product ID for %S",
                getWintunMsiPath());
        }
        else
        {
            LOG("Uninstalling WinTUN package for rollback - %S", shippedProduct);
            // Ignore the uninstall result during rollback; it's traced by
            // uninstallProduct().  Can't abort rollback, and not much use to
            // retrying.
            uninstallProduct(shippedProduct.c_str());
        }
    }
}
