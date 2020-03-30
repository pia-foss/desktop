#ifndef TUN_INL
#define TUN_INL

#ifndef TUN_LOG
#define TUN_LOG(...) ((void)0)
#endif

#include "tun_inl.h"
#include "brand.h"
#include <algorithm>
#include <Windows.h>
#include <Msi.h>
#include <MsiQuery.h>
#include <VersionHelpers.h>

#pragma comment(lib, "Msi.lib")

namespace
{
    // The product GUID must be uppercase for use with MSI functions.  This is
    // a branding parameter, so uppercase at runtime in case it is not uppercase
    // in the brand info.
    std::wstring asciiUppercase(const wchar_t *pStr)
    {
        std::wstring uppered;
        uppered.resize(std::wcslen(pStr));
        std::transform(pStr, pStr + uppered.size(), uppered.data(),
            [](wchar_t c) -> wchar_t
            {
                if(c >= 'a' && c <= 'z')
                    return c + ('A' - 'a');
                return c;
            });
        return uppered;
    }

    std::wstring g_wintunProduct = asciiUppercase(L""
#ifdef _WIN64
            BRAND_WINTUN_AMD64_PRODUCT
#else
            BRAND_WINTUN_X86_PRODUCT
#endif
        );
}

void initMsiLib(const wchar_t *pLoggingDir)
{
    // Configure MSI UI and logging.  This is global state in the MSI library.
    std::wstring msiLogPath{pLoggingDir};
    msiLogPath += L"\\" BRAND_CODE "-install-msi.log";
    unsigned msiLogResult = ::MsiEnableLogW(INSTALLLOGMODE_VERBOSE,
                                            msiLogPath.c_str(),
                                            INSTALLLOGATTRIBUTES_APPEND);
    if(msiLogResult != ERROR_SUCCESS)
        TUN_LOG("Unable to configure MSI logging - %u", msiLogResult);
    ::MsiSetInternalUI(INSTALLUILEVEL_NONE, nullptr);
}

// Test if an MSI product is installed.  (Note that this takes a product code,
// not an upgrade code.)
bool isMsiProductInstalled(const wchar_t *pProductCode)
{
    enum { BufLen = 2 };   // Looking for "5" for installed
    wchar_t value[BufLen]{};
    // On input, MsiGetProductInfoExW() wants the buffer length (including
    // the null), but on output, it returns the length of the value
    // (excluding the null, and even if it was longer than the buffer).
    DWORD valueLen{BufLen};
    unsigned queryResult = ::MsiGetProductInfoExW(pProductCode, nullptr,
                                                  MSIINSTALLCONTEXT_MACHINE,
                                                  INSTALLPROPERTY_PRODUCTSTATE,
                                                  value, &valueLen);
    if(queryResult == ERROR_SUCCESS && valueLen == 1 && value[0] == '5')
    {
        TUN_LOG("Product is currently installed - %ls", pProductCode);
        return true;
    }

    // Any other result is treated as "uninstalled", but write reasonable
    // logs for expected scenarios
    if(queryResult == ERROR_UNKNOWN_PRODUCT)
        TUN_LOG("Product is not currently installed - %ls", pProductCode);
    else if(queryResult == ERROR_SUCCESS && valueLen == 1)
    {
        TUN_LOG("Product is not currently installed, has state %C - %ls",
                value[0], pProductCode);
    }
    else
    {
        TUN_LOG("Unexpected result %u (value len %u) when checking product installation - %ls",
                queryResult, valueLen, pProductCode);
    }
    return false;
}

// Find all installed MSI products for a given upgrade code.
std::vector<std::wstring> findInstalledMsiProducts(const wchar_t *pUpgradeCode)
{
    std::vector<std::wstring> products;
    unsigned enumResult = ERROR_SUCCESS;
    for(DWORD i=0; enumResult == ERROR_SUCCESS; ++i)
    {
        std::wstring nextProduct;
        // Doc for MsiEnumRelatedProductsW() says this buffer must be 39
        // characters (GUID is 38 chars, plus one null char)
        nextProduct.resize(39);
        enumResult = ::MsiEnumRelatedProductsW(pUpgradeCode, 0, i,
                                               nextProduct.data());
        if(enumResult == ERROR_SUCCESS)
        {
            // wstring has its own null char after the buffer, don't need
            // two null chars
            nextProduct.resize(38);
            if(isMsiProductInstalled(nextProduct.c_str()))
            {
                TUN_LOG("Found installed product: %ls", nextProduct.c_str());
                products.push_back(std::move(nextProduct));
            }
            else
                TUN_LOG("Found product that is not installed: %ls", nextProduct.c_str());
        }
    }

    if(enumResult == ERROR_NO_MORE_ITEMS)
        TUN_LOG("Found %u items", products.size());
    else
    {
        TUN_LOG("Enumeration failed after finding %u items with result %u",
                products.size(), enumResult);
    }

    return products;
}

std::vector<std::wstring> findInstalledWintunProducts()
{
    return findInstalledMsiProducts(g_wintunProduct.c_str());
}

bool uninstallMsiProduct(const wchar_t *pProductCode)
{
    UINT uninstResult = ::MsiConfigureProductExW(pProductCode,
                                                 INSTALLLEVEL_DEFAULT,
                                                 INSTALLSTATE_ABSENT, L"");
    if(uninstResult == ERROR_SUCCESS)
    {
        TUN_LOG("WinTUN uninstall of %ls succeeded", pProductCode);
        return true;
    }

    TUN_LOG("WinTUN uninstall of %ls failed - %u", pProductCode, uninstResult);
    return false;
}

bool installMsiPackage(const wchar_t *pPackagePath)
{
    unsigned installResult = ::MsiInstallProductW(pPackagePath, L"");
    if(installResult == ERROR_SUCCESS)
    {
        TUN_LOG("WinTUN install succeeded for %ls", pPackagePath);
        return true;
    }

    TUN_LOG("WinTUN install of %ls failed - %u", pPackagePath, installResult);
    return false;
}

bool isWintunSupported()
{
    // We don't support installing WinTUN or using WireGuard on Windows 7.  It
    // could be made to work, but the complexity outweighs the benefit to
    // supporting 7, and usage of 7 is going down now that it is out of support.
    //
    // The WinTUN package we're distributing is only signed with SHA-256, which
    // means that it requires 7 SP1 with KB3033929 or a later update.  It's
    // possible to detect whether the user has this update (though it's not
    // pretty), but diagnosing issues surrounding this detection and the update
    // are complex - there's no straightforward way for a user to check if they
    // have SHA-2 driver support.  (Note that this is distinct from SHA-2 user
    // mode application support, which was added separately.)
    //
    // There are also IPC problems communicating with the WireGuard service on
    // Windows 7 that currently have not been addressed, these are probably
    // fixable but the issue has not been identified yet.
    return ::IsWindows8OrGreater();
}

#endif
