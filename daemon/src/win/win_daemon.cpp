// Copyright (c) 2023 Private Internet Access, Inc.
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

#include <common/src/common.h>
#line SOURCE_FILE("win/win_daemon.cpp")

#include "win_daemon.h"
#include "wfp_filters.h"
#include "win_appmanifest.h"
#include <common/src/win/win_winrtloader.h>
#include "win_interfacemonitor.h"
#include <common/src/builtin/path.h>
#include "../networkmonitor.h"
#include "win.h"
#include "brand.h"
#include "../../../extras/installer/win/tap_inl.h"
#include "../../../extras/installer/win/tun_inl.h"
#include "../../../extras/installer/win/util_inl.h" // getSystemTempPath()
#include <QDir>

#include <Msi.h>
#include <MsiQuery.h>
#include <WinDNS.h>

#pragma comment(lib, "Msi.lib")
#pragma comment(lib, "Dnsapi.lib")

#include <TlHelp32.h>
#include <Psapi.h>

namespace
{
    // Name and description for WFP filter rules
    wchar_t wfpFilterName[] = L"" PIA_PRODUCT_NAME " Firewall";
    wchar_t wfpFilterDescription[] = L"Implements privacy filtering features of " PIA_PRODUCT_NAME ".";
    wchar_t wfpProviderCtxName[] = L"" BRAND_SHORT_NAME " WFP Provider Context";
    wchar_t wfpProviderCtxDescription[] = L"" BRAND_SHORT_NAME " WFP Provider Context";
    wchar_t wfpCalloutName[] = L"" BRAND_SHORT_NAME " WFP Callout";
    wchar_t wfpCalloutDescription[] = L"" BRAND_SHORT_NAME " WFP Callout";

    // GUIDs of the callouts defined by the PIA WFP callout driver.  These must
    // match the callouts in the driver.  If you build a rebranded driver,
    // change the GUIDs in the driver and update these to match.  Otherwise,
    // keep the GUIDs for the PIA-branded driver.
    GUID PIA_WFP_CALLOUT_BIND_V4 = {0xb16b0a6e, 0x2b2a, 0x41a3, { 0x8b, 0x39, 0xbd, 0x3f, 0xfc, 0x85, 0x5f, 0xf8 } };
    GUID PIA_WFP_CALLOUT_CONNECT_V4 = { 0xb80ca14a, 0xa807, 0x4ef2, { 0x87, 0x2d, 0x4b, 0x1a, 0x51, 0x82, 0x54, 0x2 } };
    GUID PIA_WFP_CALLOUT_FLOW_ESTABLISHED_V4 = { 0x18ebe4a1, 0xa7b4, 0x4b76, { 0x9f, 0x39, 0x28, 0x57, 0x1e, 0xaa, 0x6b, 0x6 } };
    GUID PIA_WFP_CALLOUT_CONNECT_AUTH_V4 = { 0xf6e93b65, 0x5cd0, 0x4b0d, { 0xa9, 0x4c, 0x13, 0xba, 0xfd, 0x92, 0xf4, 0x1c } };
    GUID PIA_WFP_CALLOUT_IPPACKET_INBOUND_V4 = { 0x6a564cd3, 0xd14e, 0x43dc, { 0x98, 0xde, 0xa4, 0x18, 0x14, 0x4d, 0x5d, 0xd2 } };
    GUID PIA_WFP_CALLOUT_IPPACKET_OUTBOUND_V4 = { 0xb06c0a5f, 0x2b58, 0x6753, { 0x85, 0x29, 0xad, 0x8f, 0x1c, 0x51, 0x5f, 0xf5 } };

    // The shipped version of the WinTUN driver
    const WinDriverVersion shippedWintunVersion{1, 0, 0, 0};

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
            qWarning() << "Can't get length of" << QStringView{pProperty}
                << "- result" << queryResult << "-" << QStringView{pProductCode};
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
            qWarning() << "Can't get" << QStringView{pProperty} << "- result"
                << queryResult << "-" << QStringView{pProductCode};
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
                qWarning() << "Product version is not valid -" << QStringView{value};
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
        qInfo().nospace() << "Product version " << versionParts[0] << "."
            << versionParts[1] << "." << versionParts[2] << "is installed ("
            << QStringView{value} << ")";
        return {versionParts[0], versionParts[1], versionParts[2], 0};
    }

    void installWinTun()
    {
        std::vector<std::wstring> installedProducts;
        WinDriverVersion installedVersion;

        installedProducts = findInstalledWintunProducts();

        // If, somehow, more than one product with our upgrade code is installed,
        // uninstall them all (assume they are newer than the shipped package).
        // Shouldn't happen since the upgrade settings in the MSI should prevent
        // this.
        if(installedProducts.size() > 1)
        {
            qWarning() << "Found" << installedProducts.size()
                << "installed products, expected 0-1";
            installedVersion = WinDriverVersion{65535, 0, 0, 0};
        }
        else if(installedProducts.size() == 1)
        {
            auto versionStr = getProductProperty(installedProducts[0].c_str(),
                                                 INSTALLPROPERTY_VERSIONSTRING);
            installedVersion = parseProductVersion(versionStr);
        }
        // Otherwise, nothing is installed, leave installedVersion set to 0.

        // Determine whether we need to uninstall the existing driver -
        // uninstall if there is a package installed, and it is newer than the
        // version we shipped.
        //
        // The MSI package is flagged to prevent downgrades, so users don't
        // accidentally downgrade it if driver packages are sent out by support,
        // etc.  However, if PIA itself is downgraded, we do want to downgrade
        // the driver package.
        if(installedVersion == WinDriverVersion{})
        {
            qInfo() << "WinTUN package is not installed, nothing to uninstall";
        }
        else if(installedVersion == shippedWintunVersion)
        {
            qInfo() << "Version" << QString::fromStdString(installedVersion.printable())
                << "of WinTUN package is installed, do not need to install new version"
                << QString::fromStdString(shippedWintunVersion.printable());
            return;
        }
        else if(installedVersion < shippedWintunVersion)
        {
            qInfo() << "Version" << QString::fromStdString(installedVersion.printable())
                << "of WinTUN package is installed, do not need to uninstall before installing version"
                << QString::fromStdString(shippedWintunVersion.printable());
        }
        else
        {
            qInfo() << "Version" << QString::fromStdString(installedVersion.printable())
                << "of WinTUN package is installed, uninstall before installing version"
                << QString::fromStdString(shippedWintunVersion.printable());
            auto itInstalledProduct = installedProducts.begin();
            while(itInstalledProduct != installedProducts.end())
            {
                if(uninstallMsiProduct(itInstalledProduct->c_str()))
                {
                    qInfo() << "Uninstalled product" << QStringView{itInstalledProduct->c_str()};
                    ++itInstalledProduct;
                }
                else
                {
                    qWarning() << "Failed to uninstall MSI product"
                        << QStringView{itInstalledProduct->c_str()}
                        << "- aborting WinTUN installation";
                    return;
                }
            }
        }

        // Finally, install the new package
        QString packagePath = QDir::toNativeSeparators(Path::BaseDir / "wintun" / BRAND_CODE "-wintun.msi");
        if(installMsiPackage(qUtf16Printable(packagePath)))
        {
            qInfo() << "Installed WinTUN from package";
        }
        else
        {
            qWarning() << "WinTUN package installation failed";
        }
    }
}

WinUnbiasedDeadline::WinUnbiasedDeadline()
    : _expireTime{getUnbiasedTime()} // Initially expired
{
}

ULONGLONG WinUnbiasedDeadline::getUnbiasedTime() const
{
    ULONGLONG time;
    // Per doc, this can only fail if the pointer given is nullptr, which it's
    // not.
    ::QueryUnbiasedInterruptTime(&time);
    return time;
}

void WinUnbiasedDeadline::setRemainingTime(const std::chrono::microseconds &time)
{
    _expireTime = getUnbiasedTime();
    if(time > std::chrono::microseconds::zero())
    {
        // The unbiased interrupt time is in 100ns units, multiply by 10.
        _expireTime += static_cast<unsigned long long>(time.count()) * 10;
    }
}

std::chrono::microseconds WinUnbiasedDeadline::remaining() const
{
    ULONGLONG now = getUnbiasedTime();
    if(now >= _expireTime)
        return {};
    return std::chrono::microseconds{(_expireTime - now) / 10};
}

WinDaemon::WinDaemon(QObject* parent)
    : Daemon{parent},
      MessageWnd{WindowType::Invisible},
      _wfpCalloutMonitor{L"PiaWfpCallout"}
{
    kapps::net::FirewallConfig config{};
    config.daemonDataDir = Path::DaemonDataDir;
    config.resourceDir = Path::ResourceDir;
    config.executableDir = Path::ExecutableDir;
    config.productExecutables = std::vector<std::wstring>
        {
            Path::ClientExecutable,
            Path::DaemonExecutable,
            Path::OpenVPNExecutable,
            Path::SupportToolExecutable,
            Path::SsLocalExecutable,
            Path::WireguardServiceExecutable
        };
    config.resolverExecutables = std::vector<std::wstring>
        {
            Path::UnboundExecutable
        };
    config.brandInfo.pWfpFilterName = wfpFilterName;
    config.brandInfo.pWfpFilterDescription = wfpFilterDescription;
    config.brandInfo.pWfpProviderCtxName = wfpProviderCtxName;
    config.brandInfo.pWfpProviderCtxDescription = wfpProviderCtxDescription;
    config.brandInfo.pWfpCalloutName = wfpCalloutName;
    config.brandInfo.pWfpCalloutDescription = wfpCalloutDescription;

    config.brandInfo.wfpBrandProvider = BRAND_WINDOWS_WFP_PROVIDER;
    config.brandInfo.wfpBrandSublayer = BRAND_WINDOWS_WFP_SUBLAYER;

    config.brandInfo.wfpCalloutBindV4 = PIA_WFP_CALLOUT_BIND_V4;
    config.brandInfo.wfpCalloutConnectV4 = PIA_WFP_CALLOUT_CONNECT_V4;
    config.brandInfo.wfpCalloutFlowEstablishedV4 = PIA_WFP_CALLOUT_FLOW_ESTABLISHED_V4;
    config.brandInfo.wfpCalloutConnectAuthV4 = PIA_WFP_CALLOUT_CONNECT_AUTH_V4;
    config.brandInfo.wfpCalloutIppacketInboundV4 = PIA_WFP_CALLOUT_IPPACKET_INBOUND_V4;
    config.brandInfo.wfpCalloutIppacketOutboundV4 = PIA_WFP_CALLOUT_IPPACKET_OUTBOUND_V4;

    config.brandInfo.enableDnscache = [this](bool enable)
    {
        if(enable)
            _dnsCacheControl.restoreDnsCache();
        else
            _dnsCacheControl.disableDnsCache();
    };

    _pFirewall.emplace(config);

    // Qt for some reason passes Unix CA directories to OpenSSL by default on
    // Windows.  This results in the daemon attempting to load CA certificates
    // from C:\etc\ssl\, etc., which are not privileged directories on Windows.
    //
    // This seems to be an oversight.  QSslSocketPrivate::ensureCiphersAndCertsLoaded()
    // enables s_loadRootCertsOnDemand on Windows supposedly to permit fetching
    // CAs from Windows Update.  It's not clear how Windows would actually be
    // notified to fetch the certificates though, since Qt handles TLS itself
    // with OpenSSL.  The implementation of QSslCertificate::verify() does load
    // updated system certificates if this flag is set, but that still doesn't
    // mean that Windows would know to fetch a new root.
    //
    // Qt has already loaded the system CA certs as the default CAs by this
    // point, this just sets s_loadRootCertsOnDemand back to false to prevent
    // the Unix paths from being applied.
    //
    // This might break QSslCertificate::verify(), but PIA does not use this
    // since it is not provided on the Mac SecureTransport backend, we implement
    // this operation with OpenSSL directly.  Qt does not use
    // QSslCertificate::verify(), it's just provided for application use.  (It's
    // not part of the normal TLS connection establishment.)
    auto newDefaultSslConfig = QSslConfiguration::defaultConfiguration();
    newDefaultSslConfig.setCaCertificates(newDefaultSslConfig.caCertificates());
    QSslConfiguration::setDefaultConfiguration(newDefaultSslConfig);

    connect(&WinInterfaceMonitor::instance(), &WinInterfaceMonitor::interfacesChanged,
            this, &WinDaemon::checkNetworkAdapter);
    // Check the initial state now
    checkNetworkAdapter();

    // The network monitor never fails to load on Windows.
    Q_ASSERT(_pNetworkMonitor);
    // On Windows, firewall rules can change if the existing DNS servers change,
    // and the only way we can detect that is via general network changes.
    // The existing DNS server detection on Windows also depends on PIA's DNS
    // servers (since the information we can get from Windows is limited; see
    // findExistingDNS()), they are detected when applying firwall rules on
    // Windows.
    // Since the firewall rules also depend on PIA's applied DNS servers,
    connect(_pNetworkMonitor.get(), &NetworkMonitor::networksChanged,
            this, &WinDaemon::queueApplyFirewallRules);

    connect(&_wfpCalloutMonitor, &ServiceMonitor::serviceStateChanged, this,
            [this](StateModel::NetExtensionState extState)
            {
                state().netExtensionState(qEnumToString(extState));
            });
    state().netExtensionState(qEnumToString(_wfpCalloutMonitor.lastState()));
    qInfo() << "Initial callout driver state:" << state().netExtensionState();

    state().wireguardAvailable(isWintunSupported());
    // There's no way to be notified when WinTUN is installed or uninstalled.
    // pia-service.exe hints to us using the checkDriverState RPC if it performs
    // a TUN installation, and WireguardServiceBackend hints to us to re-check
    // in some cases.
    checkWintunInstallation();

    connect(this, &Daemon::aboutToConnect, this, &WinDaemon::onAboutToConnect);

    // _appMonitor.appIdsChanged() can be invoked on several different threads.
    // queueApplyFirewallRules() isn't thread safe, dispatch back to the main
    // thread.
    _appMonitor.appIdsChanged = [this]()
    {
        QMetaObject::invokeMethod(this, &Daemon::queueApplyFirewallRules,
                                  Qt::QueuedConnection);
    };

    connect(&_settings, &DaemonSettings::splitTunnelRulesChanged, this,
            &WinDaemon::updateSplitTunnelRules);
    updateSplitTunnelRules();

    connect(&_settings, &DaemonSettings::splitTunnelEnabledChanged, this, [this]()
    {
        _wfpCalloutMonitor.doManualCheck();
    });

    // Split tunnel support errors are platform-dependent, nothing else adds
    // them (otherwise we'd have to do a proper get-append-set below)
    Q_ASSERT(_state.splitTunnelSupportErrors().empty());
    // WFP has serious issues in Windows 7 RTM.  Though we still support the
    // client on Win 7 RTM, the split tunnel feature requires SP1 or newer.
    //
    // Some of the issues:
    // https://support.microsoft.com/en-us/help/981889/a-windows-filtering-platform-wfp-driver-hotfix-rollup-package-is-avail
    if(!::IsWindows7SP1OrGreater())
    {
        _state.splitTunnelSupportErrors({QStringLiteral("win_version_invalid")});
    }
}

WinDaemon::~WinDaemon()
{
    qInfo() << "WinDaemon shutdown complete";
}

std::shared_ptr<NetworkAdapter> WinDaemon::getNetworkAdapter()
{
    // For robustness, when making a connection, we always re-query for the
    // network adapter, in case the change notifications aren't 100% reliable.
    // Also update the StateModel accordingly to keep everything in sync.
    auto adapters = WinInterfaceMonitor::getDescNetworkAdapters(L"Private Internet Access Network Adapter");
    if (adapters.size() == 0)
    {
        auto remainingGracePeriod = _resumeGracePeriod.remaining().count();
        qError() << "TAP adapter is not installed, grace period time:" << remainingGracePeriod;
        // The TAP adapter usually appears to be missing following an OS resume.
        // However, this doesn't mean it isn't installed, so only report it if
        // we're not in the post-resume grace period.
        state().tapAdapterMissing(remainingGracePeriod <= 0);
        throw Error{HERE, Error::Code::NetworkAdapterNotFound,
                    QStringLiteral("Unable to locate TAP adapter")};
    }
    // Note that we _don't_ reset the resume grace period if we _do_ find the
    // TAP adapter.  We usually end up checking a few times before the "resume"
    // notification is sent by the OS, so resetting the grace period could cause
    // those checks to show spurious errors (they're normally suppressed due to
    // entering the grace period after the "suspend" notification).
    state().tapAdapterMissing(false);
    return adapters[0];
}

void WinDaemon::checkNetworkAdapter()
{
    // To check the network adapter state, just call getNetworkAdapter() and let
    // it update StateModel.  Ignore the result and any exception for a missing
    // adapter.
    try
    {
        getNetworkAdapter();
    }
    catch(const Error &)
    {
        // Ignored, indicates no adapter.
    }
}

void WinDaemon::onAboutToConnect()
{
    // Reapply split tunnel rules.  If an app updates, the executables found
    // from the rules might change (likely for UWP apps because the package
    // install paths are versioned, less likely for native apps but possible if
    // the link target changes).
    //
    // If this does happen, this means the user may have to reconnect for the
    // updated rules to apply, but this is much better than restarting the
    // service or having to make a change to the rules just to force this
    // update.
    updateSplitTunnelRules();

    // If the WFP callout driver is installed but not loaded yet, load it now.
    // The driver is loaded this way for resiliency:
    // - Loading on boot would mean that a failure in the callout driver would
    //   render the system unbootable (bluescreen on boot)
    // - Loading on first client connect would prevent the user from seeing an
    //   advertised update or installing it
    //
    // This may slow down the first connection attempt slightly, but the driver
    // does not take long to load and the resiliency gains are worth this
    // tradeoff.

    // Do a manual check of the callout state right now if needed
    _wfpCalloutMonitor.doManualCheck();

    // Skip this quickly if the driver isn't installed to avoid holding up
    // connections (don't open SCM or the service an additional time).
    // TODO - Also check master toggle for split tunnel
    if(_wfpCalloutMonitor.lastState() == StateModel::NetExtensionState::NotInstalled)
    {
        qInfo() << "Callout driver hasn't been installed, nothing to start.";
        return;
    }

    qInfo() << "Starting callout driver";
    auto startResult = startCalloutDriver(10000);
    switch(startResult)
    {
        case ServiceStatus::ServiceNotInstalled:
            // Normally the check above should detect this.
            qWarning() << "Callout driver is not installed, but monitor is in state"
                << qEnumToString(_wfpCalloutMonitor.lastState());
            break;
        case ServiceStatus::ServiceAlreadyStarted:
            qInfo() << "Callout driver is already running";
            break;
        case ServiceStatus::ServiceStarted:
            qInfo() << "Callout driver was started successfully";
            break;
        case ServiceStatus::ServiceRebootNeeded:
            // TODO - Display this in the client UI
            qWarning() << "Callout driver requires system reboot";
            break;
        default:
            qWarning() << "Callout driver couldn't be started:" << startResult;
            break;
    }
}

LRESULT WinDaemon::proc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case WM_POWERBROADCAST:
        switch(wParam)
        {
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMSUSPEND:
            // After the system resumes, allow 1 minute for the TAP adapter to
            // come back.
            //
            // This isn't perfectly reliable since it's a hard-coded timeout,
            // but there is no way to know at this point whether the TAP adapter
            // is really missing or if it's still coming back from the resume.
            // PBT_APMRESUMEAUTOMATIC typically occurs before the TAP adapter is
            // restored.  PBM_APMRESUMESUSPEND _seems_ to typically occur after
            // it is restored, but the doc indicates that this isn't sent in all
            // cases, we can't rely on it.
            //
            // This just suppresses the "TAP adapter missing" error, so the
            // failure modes are acceptable:
            // - if the adapter is really missing, we take 1 minute to actually
            //   show the error
            // - if the adapter is present but takes >1 minute to come back, we
            //   show the error incorrectly in the interim
            //
            // We also trigger the grace period for a suspend message, just in
            // case a connection attempt would occur between the suspend message
            // and the resume message.
            _resumeGracePeriod.setRemainingTime(std::chrono::minutes{1});
            checkNetworkAdapter();  // Check now in case we were showing the error already
            qInfo() << "OS suspend/resume:" << wParam;
            break;
        default:
            break;
        }
        return 0;

    default:
        return MessageWnd::proc(uMsg, wParam, lParam);
    }
}

void WinDaemon::applyFirewallRules(kapps::net::FirewallParams params)
{
    Q_ASSERT(_pFirewall);   // Class invariant
    params.excludeApps = _appMonitor.getExcludedAppIds();
    params.vpnOnlyApps = _appMonitor.getVpnOnlyAppIds();
    _pFirewall->applyRules(params);
}

QJsonValue WinDaemon::RPC_inspectUwpApps(const QJsonArray &familyIds)
{
    QJsonArray exeApps, wwaApps;

    for(const auto &family : familyIds)
    {
        auto installDirs = getWinRtLoader().adminGetInstallDirs(family.toString());
        AppExecutables appExes{};
        for(const auto &dir : installDirs)
        {
            if(!inspectUwpAppManifest(dir, appExes))
            {
                // Failed to scan a directory, skip this app, couldn't understand it
                appExes.executables.clear();
                appExes.usesWwa = false;
            }
        }

        if(appExes.usesWwa && appExes.executables.empty())
            wwaApps.push_back(family);
        else if(!appExes.usesWwa && !appExes.executables.empty())
            exeApps.push_back(family);
        else
        {
            // Otherwise, no targets were found, or both types of targets were
            // found, skip it.
            qInfo() << "Skipping app:" << family << "->" << appExes.executables.size()
                << "exes, uses wwa:" << appExes.usesWwa;
        }
    }

    QJsonObject result;
    result.insert(QStringLiteral("exe"), exeApps);
    result.insert(QStringLiteral("wwa"), wwaApps);
    return result;
}

void WinDaemon::RPC_checkDriverState()
{
    // Re-check the WFP callout state, this is only relevant on Win 10 1507
    _wfpCalloutMonitor.doManualCheck();

    checkWintunInstallation();
}

void WinDaemon::writePlatformDiagnostics(DiagnosticsFile &file)
{
    file.writeCommand("OS Version", "wmic", QStringLiteral("os get Caption,CSDVersion,BuildNumber,Version /value"));
    file.writeText("Overview", diagnosticsOverview());
    file.writeCommand("Interfaces (ipconfig)", "ipconfig", QStringLiteral("/all"));
    file.writeCommand("Routes (netstat -nr)", "netstat", QStringLiteral("-nr"));
    file.writeCommand("DNS configuration", "netsh", QStringLiteral("interface ipv4 show dnsservers"));

    for(const auto &adapter : WinInterfaceMonitor::getNetworkAdapters())
    {
        auto index = adapter->indexIpv4();
        file.writeCommand(QStringLiteral("Interface info (index=%1)").arg(index), "netsh", QStringLiteral("interface ipv4 show interface %1").arg(index));
    }

    // WFP (windows firewall) filter information. We need to process it as the raw data is XML.
    file.writeCommand("WFP filters", "netsh", QStringLiteral("wfp show filters dir = out file = -"),
        [](const QByteArray &output) { return WfpFilters(output).render(); });

    // GPU and driver info - needed to attempt to reproduce graphical issues
    // on Windows (which are pretty common due to poor OpenGL support)
    file.writeCommand("Graphics drivers", "wmic", QStringLiteral("path win32_VideoController get /format:list"));
    file.writeCommand("Network adapters", "wmic", QStringLiteral("path win32_NetworkAdapter get /format:list"));
    file.writeCommand("Network drivers", "wmic", QStringLiteral("path win32_PnPSignedDriver where 'DeviceClass=\"NET\"' get /format:list"));

    // Raw WFP filter dump, important to identify app rules (and other rules
    // that may affect the same apps) for split tunnel on Windows
    file.writeCommand("WFP filters (raw)", "netsh", QStringLiteral("wfp show filters dir = out verbose = on file = -"));

    // WFP events dumps for each excluded app.  Can diagnose issues with split
    // tunnel app rules.
    _appMonitor.dump();
    const auto &excludedApps = _appMonitor.getExcludedAppIds();
    int i=0;
    for(const auto &pAppId : excludedApps)
    {
        Q_ASSERT(pAppId);   // Guarantee of WinAppMonitor::getAppIds()

        const auto &appId = qs::toQString(pAppId->printableString());
        auto title = QStringLiteral("WFP events (bypass %1 - %2)").arg(i).arg(appId);
        auto cmdParams = QStringLiteral("wfp show netevents appid = \"%1\" file = -").arg(appId);
        file.writeCommand(title, "netsh", cmdParams);
        ++i;
    }
    const auto &vpnOnlyApps = _appMonitor.getVpnOnlyAppIds();
    i=0;
    for(const auto &pAppId : vpnOnlyApps)
    {
        Q_ASSERT(pAppId);   // Guarantee of WinAppMonitor::getAppIds()

        const auto &appId = qs::toQString(pAppId->printableString());
        auto title = QStringLiteral("WFP events (VPN-only %1 - %2)").arg(i).arg(appId);
        auto cmdParams = QStringLiteral("wfp show netevents appid = \"%1\" file = -").arg(appId);
        file.writeCommand(title, "netsh", cmdParams);
        ++i;
    }

    // Wireguard logs
    file.writeCommand("WireGuard Logs", Path::WireguardServiceExecutable,
                      QStringList{QStringLiteral("/dumplog"), Path::ConfigLogFile});

    // Whether the official WireGuard app is installed - it can sometimes cause problems
    QString wgAppExe = Path::getProgramsFolder() / QStringLiteral("WireGuard") / QStringLiteral("WireGuard.exe");
    file.writeText("Official WireGuard App installed", QFile::exists(wgAppExe) ? "yes" : "no");

    // Installed and running drivers (buggy drivers may prevent TAP installation)
    file.writeCommand("Drivers", QStringLiteral("driverquery"), {QStringLiteral("/v")});

    // DNS
    file.writeCommand("Resolve-DnsName (www.pia.com)", "powershell.exe", QStringLiteral("/C Resolve-DnsName www.privateinternetaccess.com"));
    file.writeCommand("Resolve-DnsName (-Server piadns www.pia.com)", "powershell.exe", QStringLiteral("/C Resolve-DnsName www.privateinternetaccess.com -Server %1").arg(piaModernDnsVpn()));
    file.writeCommand("ping (ping www.pia.com)", "ping", QStringLiteral("www.privateinternetaccess.com /w 1000 /n 1"));
    file.writeCommand("ping (ping piadns)", "ping", QStringLiteral("%1 /w 1000 /n 1").arg(piaModernDnsVpn()));

    auto installLog = getSystemTempPath();
    // It's possible that getSystemTempPath() could fail if TEMP was not set in
    // system variables for some reason.  Leave installLog empty rather than
    // trying to read '/pia-install.log'
    if(!installLog.empty())
        installLog += L"\\" BRAND_CODE "-install.log";

    file.writeCommand("Installer log", "cmd.exe",
        QString::fromStdWString(LR"(/C "type ")" + installLog + LR"("")"));
}

void WinDaemon::checkWintunInstallation()
{
    const auto &installedProducts = findInstalledWintunProducts();
    qInfo() << "WinTUN installed products:" << installedProducts.size();
    for(const auto &product : installedProducts)
        qInfo() << "-" << product;
    _state.wintunMissing(installedProducts.empty());
}

void WinDaemon::updateSplitTunnelRules()
{
    // Try to load the link reader; this can fail.
    nullable_t<kapps::core::WinLinkReader> linkReader;
    try
    {
        linkReader.emplace();
    }
    catch(const std::exception &ex)
    {
        qWarning() << "Unable to resolve shell links -" << ex.what();
        // Eat error and continue
    }

    // Extract actual executable paths from all the split tunnel rules - resolve
    // UWP apps, links, etc.  Use an AppExecutables for each so we can
    // accumulate all calls to inspectUwpAppManifest() as well as normal apps.
    // We don't care about usesWwa here, that is handled when rules are created.
    AppExecutables excludedExes;
    AppExecutables vpnOnlyExes;
    // Guess that there will usually not be more executables than total rules
    excludedExes.executables.reserve(_settings.splitTunnelRules().size());
    vpnOnlyExes.executables.reserve(_settings.splitTunnelRules().size());

    for(const auto &rule : _settings.splitTunnelRules())
    {
        AppExecutables *pExes{};
        if(rule.mode() == QStringLiteral("exclude"))
            pExes = &excludedExes;
        else if(rule.mode() == QStringLiteral("include"))
            pExes = &vpnOnlyExes;
        else
            continue;   // Ignore any future rule types

        Q_ASSERT(pExes);    // Postcondition of above

        // UWP apps can have more than one target
        if(rule.path().startsWith(uwpPathPrefix))
        {
            auto installDirs = getWinRtLoader().adminGetInstallDirs(rule.path().mid(uwpPathPrefix.size()));
            // Inspect all of the install directories.  It's too late for us to
            // do anything if this fails, just grab all the executables we can
            // find.
            for(const auto &dir : installDirs)
                inspectUwpAppManifest(dir, *pExes);
        }
        else
        {
            // If the client gave us a link target, use that.
            if(!rule.linkTarget().isEmpty())
                pExes->executables.insert(rule.linkTarget().toStdWString());
            // Otherwise, if the rule path is a link, still try to resolve it.
            // This may not work if we are not in the correct user session
            // though.
            else if(rule.path().endsWith(QStringLiteral(".lnk"), Qt::CaseSensitivity::CaseInsensitive))
            {
                auto pathWStr = rule.path().toStdWString();
                std::wstring linkTarget;
                if(linkReader && linkReader->loadLink(pathWStr))
                    linkTarget = linkReader->getLinkTarget(pathWStr);
                if(linkTarget.empty())
                    qWarning() << "Can't find link target for" << pathWStr;
                else
                {
                    qInfo() << "Resolved app link" << pathWStr << "->"
                        << linkTarget;
                    pExes->executables.insert(linkTarget);
                }
            }
            else
                pExes->executables.insert(rule.path().toStdWString());
        }
    }

    _appMonitor.setSplitTunnelRules(excludedExes.executables, vpnOnlyExes.executables);
}

class TraceMemSize : public kapps::core::OStreamInsertable<TraceMemSize>
{
public:
    TraceMemSize(std::size_t mem) : _mem{mem} {}

public:
    void trace(std::ostream &os) const
    {
        const std::array<const char *, 3> units
        {{
            "B",
            "KiB",
            "MiB"
        }};

        int unitIdx{0};
        std::size_t memInUnits{_mem};
        while(unitIdx+1 < units.size() && memInUnits > 1024)
        {
            ++unitIdx;
            memInUnits /= 1024;
        }

        os << memInUnits << ' ' << units[unitIdx];
    }

private:
    std::size_t _mem;
};

void WinDaemon::wireguardServiceFailed()
{
    // If the connection failed after the WG service was started, check whether
    // WinTUN is installed, it might be due to a lack of the driver.  We don't
    // do this for other failures, there's no need to do it for every attempt if
    // we can't reach the server at all to authenticate.
    checkWintunInstallation();
}

void WinDaemon::wireguardConnectionSucceeded()
{
    // Only re-check WinTUN if we thought it was missing.  It is likely present
    // now since the connection succeeded.  This avoids doing this
    // potentially-expensive check in the normal case.
    if(_state.wintunMissing())
        checkWintunInstallation();
}

void WinDaemon::handlePendingWinTunInstall()
{
    // If PIA was recently installed, do WinTUN installation now.  The installer
    // creates a file to flag this condition; this applies to upgrades,
    // downgrades, and reinstalls.
    //
    // Installation of WinTUN is performed by the daemon itself, not the
    // installer, because it can't be installed in Safe Mode (the msiservice
    // service can't be started).
    //
    // We use an explicit flag instead of checking the last-used version so a
    // reinstall will also cause WinTUN to be re-checked.
    if(QFile::exists(Path::DaemonDataDir / ".need-wintun-install"))
    {
        qInfo() << "MSI install needed, install WinTUN now";
        QFile::remove(Path::DaemonDataDir / ".need-wintun-install");
        installWinTun();
        checkWintunInstallation();
    }
}
