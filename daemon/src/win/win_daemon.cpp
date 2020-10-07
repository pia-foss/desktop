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

#include "common.h"
#line SOURCE_FILE("win/win_daemon.cpp")

#include "win_daemon.h"
#include "wfp_filters.h"
#include "win_appmanifest.h"
#include "win/win_winrtloader.h"
#include "win_interfacemonitor.h"
#include "path.h"
#include "networkmonitor.h"
#include "win.h"
#include "brand.h"
#include "../../../extras/installer/win/tap_inl.h"
#include "../../../extras/installer/win/tun_inl.h"
#include <QDir>

#include <Msi.h>
#include <MsiQuery.h>
#include <WinDNS.h>

#pragma comment(lib, "Msi.lib")
#pragma comment(lib, "Dnsapi.lib")

#include <TlHelp32.h>
#include <Psapi.h>

// The 'bind' callout GUID is the GUID used in 1.7 and earlier; the WFP callout
// only handled the bind layer in those releases.
GUID PIA_WFP_CALLOUT_BIND_V4 = {0xb16b0a6e, 0x2b2a, 0x41a3, { 0x8b, 0x39, 0xbd, 0x3f, 0xfc, 0x85, 0x5f, 0xf8 } };
GUID PIA_WFP_CALLOUT_CONNECT_V4 = { 0xb80ca14a, 0xa807, 0x4ef2, { 0x87, 0x2d, 0x4b, 0x1a, 0x51, 0x82, 0x54, 0x2 } };
GUID PIA_WFP_CALLOUT_FLOW_ESTABLISHED_V4 = { 0x18ebe4a1, 0xa7b4, 0x4b76, { 0x9f, 0x39, 0x28, 0x57, 0x1e, 0xaa, 0x6b, 0x6 } };
GUID PIA_WFP_CALLOUT_CONNECT_AUTH_V4 = { 0xf6e93b65, 0x5cd0, 0x4b0d, { 0xa9, 0x4c, 0x13, 0xba, 0xfd, 0x92, 0xf4, 0x1c } };
GUID PIA_WFP_CALLOUT_IPPACKET_INBOUND_V4 = { 0x6a564cd3, 0xd14e, 0x43dc, { 0x98, 0xde, 0xa4, 0x18, 0x14, 0x4d, 0x5d, 0xd2 } };
GUID PIA_WFP_CALLOUT_IPPACKET_OUTBOUND_V4 = { 0xb06c0a5f, 0x2b58, 0x6753, { 0x85, 0x29, 0xad, 0x8f, 0x1c, 0x51, 0x5f, 0xf5 } };

namespace
{
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

void WinRouteManager::createRouteEntry(MIB_IPFORWARD_ROW2 &route, const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric) const
{
    InitializeIpForwardEntry(&route);
    NET_LUID luid{};

    luid.Value = static_cast<ULONG64>(interfaceName.toULongLong());
    route.InterfaceLuid = luid;

    const auto subnetPair = QHostAddress::parseSubnet(subnet);
    const QHostAddress &subnetIp = subnetPair.first;
    const int subnetPrefixLength = subnetPair.second;

    // Destination subnet
    route.DestinationPrefix.Prefix.si_family = AF_INET;
    route.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr = htonl(subnetIp.toIPv4Address());
    route.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
    route.DestinationPrefix.PrefixLength = subnetPrefixLength;

    // Router address (next hop)
    route.NextHop.Ipv4.sin_addr.s_addr = htonl(QHostAddress{gatewayIp}.toIPv4Address());
    route.NextHop.Ipv4.sin_family = AF_INET;

    route.Metric = metric;
}

void WinRouteManager::addRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric) const
{
    MIB_IPFORWARD_ROW2 route{};
    createRouteEntry(route, subnet, gatewayIp, interfaceName, metric);

    qInfo() << "Adding route for" << subnet << "via" << interfaceName
        << "with metric:" << metric;
    // Add the routing entry
    auto routeResult = CreateIpForwardEntry2(&route);
    if(routeResult != NO_ERROR)
    {
        qWarning() << "Could not create route for" << subnet << "-"
            << WinErrTracer{routeResult};
    }
}

void WinRouteManager::removeRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName) const
{
    MIB_IPFORWARD_ROW2 route{};
    createRouteEntry(route, subnet, gatewayIp, interfaceName, 0);
    qInfo() << "Removing route for" << subnet << "via" << interfaceName;

    // Delete the routing entry
    if(DeleteIpForwardEntry2(&route) != NO_ERROR)
    {
        qWarning() << "Could not delete route for" << subnet;
    }
}

WinDaemon::WinDaemon(QObject* parent)
    : Daemon{parent}
    , MessageWnd(WindowType::Invisible)
    , _firewall(new FirewallEngine(this))
    , _unboundAppId{nullptr, Path::UnboundExecutable}
#if INCLUDE_FEATURE_HANDSHAKE
    , _hnsdAppId{nullptr, Path::HnsdExecutable}
#endif
    , _lastSplitParams{}
    , _wfpCalloutMonitor{L"PiaWfpCallout"}
    , _subnetBypass{std::make_unique<WinRouteManager>()}
{
    _filters = FirewallFilters{};
    _filterAdapterLuid = 0;

    if (!_firewall->open() || !_firewall->installProvider())
    {
        qCritical() << "Unable to initialize WFP firewall";
        delete _firewall;
        _firewall = nullptr;
    }
    else
    {
        _firewall->removeAll();
    }

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
            [this](DaemonState::NetExtensionState extState)
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

    connect(&_appMonitor, &WinAppMonitor::appIdsChanged, this,
            &WinDaemon::queueApplyFirewallRules);

    connect(&_settings, &DaemonSettings::splitTunnelRulesChanged, this,
            [this](){_appMonitor.setSplitTunnelRules(_settings.splitTunnelRules());});
    _appMonitor.setSplitTunnelRules(_settings.splitTunnelRules());

    connect(&_settings, &DaemonSettings::splitTunnelEnabledChanged, this, [this]()
    {
        _wfpCalloutMonitor.doManualCheck();
    });

    _clientMemTraceTimer.setInterval(msec(std::chrono::minutes(5)));
    connect(&_clientMemTraceTimer, &QTimer::timeout, this, &WinDaemon::traceClientMemory);

    connect(this, &WinDaemon::firstClientConnected, this, [this]()
    {
        _clientMemTraceTimer.start();
        traceClientMemory();
    });
    connect(this, &WinDaemon::lastClientDisconnected, this, [this]()
    {
        _clientMemTraceTimer.stop();
    });
}

WinDaemon::~WinDaemon()
{
#define deactivateFilter(filterVariable, removeCondition) \
    do { \
        /* Remove existing rule if necessary */ \
        if ((removeCondition) && filterVariable != zeroGuid) \
        { \
            if (!_firewall->remove(filterVariable)) { \
                qWarning() << "Failed to remove WFP filter" << #filterVariable; \
            } \
            filterVariable = {zeroGuid}; \
        } \
    } \
    while(false)
#define activateFilter(filterVariable, addCondition, ...) \
    do { \
        /* Add new rule if necessary */ \
        if ((addCondition) && filterVariable == zeroGuid) \
        { \
            if ((filterVariable = _firewall->add(__VA_ARGS__)) == zeroGuid) { \
                reportError(Error(HERE, Error::FirewallRuleFailed, { QStringLiteral(#filterVariable) })); \
            } \
        } \
    } \
    while(false)
#define updateFilter(filterVariable, removeCondition, addCondition, ...) \
    do { \
        deactivateFilter(_filters.filterVariable, removeCondition); \
        activateFilter(_filters.filterVariable, addCondition, __VA_ARGS__); \
    } while(false)
#define updateBooleanFilter(filterVariable, enableCondition, ...) \
    do { \
        const bool enable = (enableCondition); \
        updateFilter(filterVariable, !enable, enable, __VA_ARGS__); \
    } while(false)
#define updateBooleanInvalidateFilter(filterVariable, enableCondition, invalidateCondition, ...) \
    do { \
        const bool enable = (enableCondition); \
        const bool disable = !enable || (invalidateCondition); \
        updateFilter(filterVariable, disable, enable, __VA_ARGS__); \
    } while(false)
#define filterActive(filterVariable) (_filters.filterVariable != zeroGuid)

    if (_firewall)
    {
        qInfo() << "Cleaning up WFP objects";

        if (_filters.ipInbound != zeroGuid)
        {
            qInfo() << "deactivate IpInbound object";
            deactivateFilter(_filters.ipInbound, true);
        }

        if (_filters.splitCalloutIpInbound != zeroGuid)
        {
            qInfo() << "deactivate IpInbound callout object";
            deactivateFilter(_filters.splitCalloutIpInbound, true);
        }

        if (_filters.ipOutbound != zeroGuid)
        {
            qInfo() << "deactivate IpOutbound object";
            deactivateFilter(_filters.ipOutbound, true);
        }

        if (_filters.splitCalloutIpOutbound != zeroGuid)
        {
            qInfo() << "deactivate IpOutbound callout object";
            deactivateFilter(_filters.splitCalloutIpOutbound, true);
        }

        _firewall->removeAll();
        _firewall->uninstallProvider();
        _firewall->checkLeakedObjects();
    }
    else
        qInfo() << "Firewall was not initialized, nothing to clean up";

    qInfo() << "WinDaemon shutdown complete";
}

std::shared_ptr<NetworkAdapter> WinDaemon::getNetworkAdapter()
{
    // For robustness, when making a connection, we always re-query for the
    // network adapter, in case the change notifications aren't 100% reliable.
    // Also update the DaemonState accordingly to keep everything in sync.
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
    // it update DaemonState.  Ignore the result and any exception for a missing
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
    _appMonitor.setSplitTunnelRules(_settings.splitTunnelRules());

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
    if(_wfpCalloutMonitor.lastState() == DaemonState::NetExtensionState::NotInstalled)
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

std::vector<quint32> WinDaemon::findExistingDNS(const std::vector<quint32> &piaDNSServers)
{
    std::vector<quint32> newDNSServers;

    // What we'd really like to do here is get the DNS servers for the primary
    // network interface.  Unfortunately, there is no API to do that, and even
    // parsing `netsh interface ip show dnsservers` won't work since that relies
    // on the DNSCache service (which PIA must stop to implement split tunnel
    // DNS).
    //
    // The best we can do is to get all DNS servers, which may include PIA's,
    // but the preexisting servers will still be there.  Then filter out PIA's
    // servers.
    //
    // If no DNS servers remain, then assume that the existing DNS servers were
    // the same as PIA's - use PIA's DNS servers for bypass apps too.
    //
    // There are a few ways that this can be incorrect if there were no existing
    // DNS servers, or the existing DNS servers were a subset of PIA's, etc.
    // Given the assumption that alternate DNS servers configured on the same
    // adapter are equivalent, it will behave reasonably, generally just adding
    // or removing some equivalent DNS servers.
    //
    // The only way this can really significantly fail is if the user has
    // multiple physical adapters, with different DNS servers configured on
    // each, and where the primary adapter's DNS servers are the same as PIA's.
    // In this case, we would incorrectly treat the secondary adapter's servers
    // as the preexisting DNS, and likely use them on the primary adapter.

    std::aligned_storage_t<1024, alignof(IP4_ARRAY)> dnsAddrBuf;
    DWORD dnsBufLen = sizeof(dnsAddrBuf);
    DNS_STATUS status = DnsQueryConfig(DnsConfigDnsServerList, 0,
                                       NULL, NULL, &dnsAddrBuf, &dnsBufLen);
    if(status == 0) // Success
    {
        const IP4_ARRAY &dnsServers = *reinterpret_cast<const IP4_ARRAY*>(&dnsAddrBuf);
        newDNSServers.reserve(dnsServers.AddrCount);
        qInfo() << "Got" << dnsServers.AddrCount
            << "existing DNS servers";
        for(DWORD i=0; i<dnsServers.AddrCount; ++i)
        {
            quint32 dnsServerAddr{ntohl(dnsServers.AddrArray[i])};
            // Check if this was one of ours.  We only apply up to 2 DNS
            // servers, so a linear search through the vector is fine.
            bool setByPia = std::find(piaDNSServers.begin(), piaDNSServers.end(),
                                      dnsServerAddr) != piaDNSServers.end();
            qInfo() << " -" << i << "-" << Ipv4Address{dnsServerAddr}
                << (setByPia ? "(ours)" : "");
            if(!setByPia)
                newDNSServers.push_back(dnsServerAddr);
        }

        // If no DNS servers remain, then the preexisting DNS servers were
        // likely the same as PIA's - assume they were.
        if(newDNSServers.empty())
        {
            qInfo() << "All DNS servers appear to be ours, assuming preexisting servers were the same";
            newDNSServers = piaDNSServers;
        }
    }
    else
    {
        qWarning() << "Could not get existing DNS servers - error" << status;
    }

    return newDNSServers;
}

static void logFilter(const char* filterName, int currentState, bool enableCondition, bool invalidateCondition = false)
{
    if (enableCondition ? currentState != 1 || invalidateCondition : currentState != 0)
        qInfo("%s: %s -> %s", filterName, currentState == 1 ? "ON" : currentState == 0 ? "OFF" : "MIXED", enableCondition ? "ON" : "OFF");
    else
        qInfo("%s: %s", filterName, enableCondition ? "ON" : "OFF");
}

static void logFilter(const char* filterName, const GUID& filterVariable, bool enableCondition, bool invalidateCondition = false)
{
    logFilter(filterName, filterVariable == zeroGuid ? 0 : 1, enableCondition, invalidateCondition);
}

template<class FilterObjType, size_t N>
static void logFilter(const char* filterName, const FilterObjType (&filterVariables)[N], bool enableCondition, bool invalidateCondition = false)
{
    int state = filterVariables[0] == zeroGuid ? 0 : 1;
    for (size_t i = 1; i < N; i++)
    {
        int s = filterVariables[i] == zeroGuid ? 0 : 1;
        if (s != state)
        {
            state = 2;
            break;
        }
    }
    logFilter(filterName, state, enableCondition, invalidateCondition);
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

void WinDaemon::applyFirewallRules(const FirewallParams& params)
{
    if (!_firewall)
        return;

    QStringList effectiveDnsServers;
    if(params._connectionSettings)
        effectiveDnsServers = params._connectionSettings->getDnsServers();

    auto networkAdapter = std::static_pointer_cast<WinNetworkAdapter>(params.adapter);

    FirewallTransaction tx(_firewall);


#define deactivateFilter(filterVariable, removeCondition) \
    do { \
        /* Remove existing rule if necessary */ \
        if ((removeCondition) && filterVariable != zeroGuid) \
        { \
            if (!_firewall->remove(filterVariable)) { \
                qWarning() << "Failed to remove WFP filter" << #filterVariable; \
            } \
            filterVariable = {zeroGuid}; \
        } \
    } \
    while(false)
#define activateFilter(filterVariable, addCondition, ...) \
    do { \
        /* Add new rule if necessary */ \
        if ((addCondition) && filterVariable == zeroGuid) \
        { \
            if ((filterVariable = _firewall->add(__VA_ARGS__)) == zeroGuid) { \
                reportError(Error(HERE, Error::FirewallRuleFailed, { QStringLiteral(#filterVariable) })); \
            } \
        } \
    } \
    while(false)
#define updateFilter(filterVariable, removeCondition, addCondition, ...) \
    do { \
        deactivateFilter(_filters.filterVariable, removeCondition); \
        activateFilter(_filters.filterVariable, addCondition, __VA_ARGS__); \
    } while(false)
#define updateBooleanFilter(filterVariable, enableCondition, ...) \
    do { \
        const bool enable = (enableCondition); \
        updateFilter(filterVariable, !enable, enable, __VA_ARGS__); \
    } while(false)
#define updateBooleanInvalidateFilter(filterVariable, enableCondition, invalidateCondition, ...) \
    do { \
        const bool enable = (enableCondition); \
        const bool disable = !enable || (invalidateCondition); \
        updateFilter(filterVariable, disable, enable, __VA_ARGS__); \
    } while(false)
#define filterActive(filterVariable) (_filters.filterVariable != zeroGuid)

    // Firewall rules, listed in order of ascending priority (as if the last
    // matching rule applies, but note that it is the priority argument that
    // actually determines precedence).

    // As a bit of an exception to the normal firewall rule logic, the WFP
    // rules handle the blockIPv6 rule by changing the priority of the IPv6
    // part of the killswitch rule instead of having a dedicated IPv6 block.

    // Block all other traffic when killswitch is enabled. If blockIPv6 is
    // true, block IPv6 regardless of killswitch state.
    logFilter("blockAll(IPv4)", _filters.blockAll[0], params.blockAll);
    updateBooleanFilter(blockAll[0], params.blockAll,                     EverythingFilter<FWP_ACTION_BLOCK, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(0));
    logFilter("blockAll(IPv6)", _filters.blockAll[1], params.blockAll || params.blockIPv6);
    updateBooleanFilter(blockAll[1], params.blockAll || params.blockIPv6, EverythingFilter<FWP_ACTION_BLOCK, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(params.blockIPv6 ? 4 : 0));

    // Exempt traffic going over the VPN adapter.  This is the TAP adapter for
    // OpenVPN, or the WinTUN adapter for Wireguard.
    UINT64 luid = networkAdapter ? networkAdapter->luid() : 0;
    logFilter("allowVPN", _filters.permitAdapter, luid && params.allowVPN, luid != _filterAdapterLuid);
    updateBooleanInvalidateFilter(permitAdapter[0], luid && params.allowVPN, luid != _filterAdapterLuid, InterfaceFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(luid, 2));
    updateBooleanInvalidateFilter(permitAdapter[1], luid && params.allowVPN, luid != _filterAdapterLuid, InterfaceFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(luid, 2));
    _filterAdapterLuid = luid;
    // Note: This is where the IPv6 block rule is ordered if blockIPv6 is true.

    // Exempt DHCP traffic.
    logFilter("allowDHCP", _filters.permitDHCP, params.allowDHCP);
    updateBooleanFilter(permitDHCP[0], params.allowDHCP, DHCPFilter<FWP_ACTION_PERMIT, FWP_IP_VERSION_V4>(6));
    updateBooleanFilter(permitDHCP[1], params.allowDHCP, DHCPFilter<FWP_ACTION_PERMIT, FWP_IP_VERSION_V6>(6));

    // Permit LAN traffic depending on settings
    logFilter("allowLAN", _filters.permitLAN, params.allowLAN);
    updateBooleanFilter(permitLAN[0], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(QStringLiteral("192.168.0.0/16"), 8));
    updateBooleanFilter(permitLAN[1], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(QStringLiteral("172.16.0.0/12"), 8));
    updateBooleanFilter(permitLAN[2], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(QStringLiteral("10.0.0.0/8"), 8));
    updateBooleanFilter(permitLAN[3], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(QStringLiteral("224.0.0.0/4"), 8));
    updateBooleanFilter(permitLAN[4], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(QStringLiteral("169.254.0.0/16"), 8));
    updateBooleanFilter(permitLAN[5], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(QStringLiteral("255.255.255.255/32"), 8));
    updateBooleanFilter(permitLAN[6], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(QStringLiteral("fc00::/7"), 8));
    updateBooleanFilter(permitLAN[7], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(QStringLiteral("fe80::/10"), 8));
    updateBooleanFilter(permitLAN[8], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(QStringLiteral("ff00::/8"), 8));
    // Permit the IPv6 global Network Prefix - this allows on-link IPv6 hosts to communicate using their global IPs
    // which is more common in practice than link-local
    updateBooleanFilter(permitLAN[9], params.netScan.hasIpv6() && params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(
            // First 64 bits of a global IPv6 IP is the Network Prefix.
            QStringLiteral("%1/64").arg(params.netScan.ipAddress6()), 8));

    // Poke holes in firewall for the bypass subnets for Ipv4 and Ipv6
    updateAllBypassSubnetFilters(params);

    // Add rules to block non-PIA DNS servers if connected and DNS leak protection is enabled
    logFilter("blockDNS", _filters.blockDNS, params.blockDNS);
    updateBooleanFilter(blockDNS[0], params.blockDNS, DNSFilter<FWP_ACTION_BLOCK, FWP_IP_VERSION_V4>(10));
    updateBooleanFilter(blockDNS[1], params.blockDNS, DNSFilter<FWP_ACTION_BLOCK, FWP_IP_VERSION_V6>(10));

    QString dnsServers[2] { effectiveDnsServers.value(0), effectiveDnsServers.value(1) }; // default-constructed if missing
    logFilter("allowDNS(1)", _filters.permitDNS[0], params.blockDNS && !dnsServers[0].isEmpty(), _dnsServers[0] != dnsServers[0]);
    updateBooleanInvalidateFilter(permitDNS[0], params.blockDNS && !dnsServers[0].isEmpty(), _dnsServers[0] != dnsServers[0], IPAddressFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(dnsServers[0], 14));
    _dnsServers[0] = dnsServers[0];
    logFilter("allowDNS(2)", _filters.permitDNS[1], params.blockDNS && !dnsServers[1].isEmpty(), _dnsServers[1] != dnsServers[1]);
    updateBooleanInvalidateFilter(permitDNS[1], params.blockDNS && !dnsServers[1].isEmpty(), _dnsServers[1] != dnsServers[1], IPAddressFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(dnsServers[1], 14));
    _dnsServers[1] = dnsServers[1];

    // Always permit traffic from known applications.
    logFilter("allowPIA", _filters.permitPIA, params.allowPIA);
    updateBooleanFilter(permitPIA[0], params.allowPIA, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::ClientExecutable, 15));
    updateBooleanFilter(permitPIA[1], params.allowPIA, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::DaemonExecutable, 15));
    updateBooleanFilter(permitPIA[2], params.allowPIA, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::OpenVPNExecutable, 15));
    updateBooleanFilter(permitPIA[3], params.allowPIA, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::SupportToolExecutable, 15));
    updateBooleanFilter(permitPIA[4], params.allowPIA, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::SsLocalExecutable, 15));
    // Although the Wireguard service creates its own 'permit' rules, they're in
    // a different sublayer, we need to permit it through our sublayer too.  See
    // wireguardservicebackend.cpp
    updateBooleanFilter(permitPIA[5], params.allowPIA, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::WireguardServiceExecutable, 15));

    // Local resolver related filters
    // (1) First we block everything coming from the resolver processes
    logFilter("allowResolver (block everything)", _filters.blockResolvers, luid && params.allowResolver, luid != _filterAdapterLuid);
    updateBooleanInvalidateFilter(blockResolvers[0], luid && params.allowResolver, luid != _filterAdapterLuid, ApplicationFilter<FWP_ACTION_BLOCK, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::UnboundExecutable, 14));
#if INCLUDE_FEATURE_HANDSHAKE
    updateBooleanInvalidateFilter(blockResolvers[1], luid && params.allowResolver, luid != _filterAdapterLuid, ApplicationFilter<FWP_ACTION_BLOCK, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::HnsdExecutable, 14));
#endif

    // (2) Next we poke a hole in this block but only allow data that goes across the tunnel
    logFilter("allowResolver (tunnel traffic)", _filters.permitResolvers, luid && params.allowResolver, luid != _filterAdapterLuid);
    updateBooleanInvalidateFilter(permitResolvers[0], luid && params.allowResolver, luid != _filterAdapterLuid, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::UnboundExecutable, 15,
        Condition<FWP_UINT64>{FWPM_CONDITION_IP_LOCAL_INTERFACE, FWP_MATCH_EQUAL, &luid},
        Condition<FWP_UINT16>{FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 53}
    ));
#if INCLUDE_FEATURE_HANDSHAKE
    updateBooleanInvalidateFilter(permitResolvers[1], luid && params.allowResolver, luid != _filterAdapterLuid, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::HnsdExecutable, 15,
        Condition<FWP_UINT64>{FWPM_CONDITION_IP_LOCAL_INTERFACE, FWP_MATCH_EQUAL, &luid},

        // OR'ing of conditions is done automatically when you have 2 or more consecutive conditions of the same fieldId.
        Condition<FWP_UINT16>{FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 53},

        // 13038 is the Handshake control port
        Condition<FWP_UINT16>{FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 13038}
    ));
#endif

    // Always permit loopback traffic, including IPv6.
    logFilter("allowLoopback", _filters.permitLocalhost, params.allowLoopback);
    updateBooleanFilter(permitLocalhost[0], params.allowLoopback, LocalhostFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(15));
    updateBooleanFilter(permitLocalhost[1], params.allowLoopback, LocalhostFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(15));

    // Get the current set of excluded app IDs.  If they've changed we recreate
    // all app rules, but if they stay the same we don't recreate them.
    std::set<const AppIdKey*, PtrValueLess> newExcludedApps, newVpnOnlyApps;
    QString splitTunnelPhysIp{""};
    unsigned splitTunnelPhysNetPrefix{0};
    if(params.enableSplitTunnel)
    {
        newExcludedApps = _appMonitor.getExcludedAppIds();
        newVpnOnlyApps = _appMonitor.getVpnOnlyAppIds();

        if(!params.defaultRoute)
        {
            // Put hnsd in the VPN since we did not use the VPN as the default
            // route
            newVpnOnlyApps.insert(&_unboundAppId);
#if INCLUDE_FEATURE_HANDSHAKE
            newVpnOnlyApps.insert(&_hnsdAppId);
#endif
        }

        splitTunnelPhysIp = params.netScan.ipAddress();
        splitTunnelPhysNetPrefix = params.netScan.prefixLength();
    }

    qInfo() << "Number of excluded apps" << newExcludedApps.size();
    qInfo() << "Number of vpnOnly apps" << newVpnOnlyApps.size();

    SplitTunnelFirewallParams newSplitParams{};
    newSplitParams._physicalIp = splitTunnelPhysIp;
    newSplitParams._physicalNetPrefix = splitTunnelPhysNetPrefix;
    newSplitParams._tunnelIp = _state.tunnelDeviceLocalAddress();
    newSplitParams._isConnected = params.isConnected;
    newSplitParams._hasConnected = params.hasConnected;
    newSplitParams._vpnDefaultRoute = params.defaultRoute;
    newSplitParams._blockDNS = params.blockDNS;
    newSplitParams._forceVpnOnlyDns = newSplitParams._forceBypassDns = false;
    if(params._connectionSettings)
    {
        newSplitParams._forceVpnOnlyDns = params._connectionSettings->forceVpnOnlyDns();
        newSplitParams._forceBypassDns = params._connectionSettings->forceBypassDns();
    }
    newSplitParams._effectiveDnsServers.reserve(effectiveDnsServers.size());
    for(const auto &effectiveDnsServer : effectiveDnsServers)
    {
        Ipv4Address serverIp{effectiveDnsServer};
        if(serverIp != Ipv4Address{})
            newSplitParams._effectiveDnsServers.push_back(serverIp.address());
    }
    newSplitParams._existingDnsServers = findExistingDNS(newSplitParams._effectiveDnsServers);

    reapplySplitTunnelFirewall(newSplitParams, newExcludedApps, newVpnOnlyApps);

    // Update subnet bypass routes
    _subnetBypass.updateRoutes(params);

    tx.commit();
}

void WinDaemon::updateAllBypassSubnetFilters(const FirewallParams &params)
{
    if(params.enableSplitTunnel)
    {
        if(params.bypassIpv4Subnets != _bypassIpv4Subnets)
            updateBypassSubnetFilters(params.bypassIpv4Subnets, _bypassIpv4Subnets, _subnetBypassFilters4, FWP_IP_VERSION_V4);

        if(params.bypassIpv6Subnets != _bypassIpv6Subnets)
            updateBypassSubnetFilters(params.bypassIpv6Subnets, _bypassIpv6Subnets, _subnetBypassFilters6, FWP_IP_VERSION_V6);
    }
    else
    {
        if(!_bypassIpv4Subnets.isEmpty())
            updateBypassSubnetFilters({}, _bypassIpv4Subnets, _subnetBypassFilters4, FWP_IP_VERSION_V4);

        if(!_bypassIpv6Subnets.isEmpty())
            updateBypassSubnetFilters({}, _bypassIpv6Subnets, _subnetBypassFilters6, FWP_IP_VERSION_V6);
    }
}

void WinDaemon::updateBypassSubnetFilters(const QSet<QString> &subnets, QSet<QString> &oldSubnets, std::vector<WfpFilterObject> &subnetBypassFilters, FWP_IP_VERSION ipVersion)
{
    for (auto &filter : subnetBypassFilters)
        deactivateFilter(filter, true);

    // If we have any IPv6 subnets we need to also whitelist IPv6 link-local and broadcast ranges
    // required by IPv6 Neighbor Discovery
    auto adjustedSubnets = subnets;
    if(ipVersion == FWP_IP_VERSION_V6 && !subnets.isEmpty())
    {
        adjustedSubnets << QStringLiteral("fe80::/10");
        adjustedSubnets << QStringLiteral("ff00::/8");
    }

    subnetBypassFilters.resize(adjustedSubnets.size());

    int index{0};
    for(auto it = adjustedSubnets.begin(); it != adjustedSubnets.end(); ++it, ++index)
    {
        if(ipVersion == FWP_IP_VERSION_V6)
        {
            qInfo() << "Creating Subnet ipv6 rule" << *it;
            activateFilter(subnetBypassFilters[index], true,
                IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(*it, 10));
        }
        else
        {
            qInfo() << "Creating Subnet ipv4 rule" << *it;
            activateFilter(subnetBypassFilters[index], true,
                IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(*it, 10));
        }
    }

    // Update the bypass subnets
    oldSubnets = subnets;
}

void WinDaemon::removeSplitTunnelAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                            const QString &traceType)
{
    for(auto &oldApp : apps)
    {
        qInfo() << "remove" << traceType << "app filters:"
            << QStringView{reinterpret_cast<const wchar_t*>(oldApp.first.data()),
                           static_cast<qsizetype>(oldApp.first.size() / sizeof(wchar_t))};
        deactivateFilter(oldApp.second.splitAppBind, true);
        deactivateFilter(oldApp.second.splitAppConnect, true);
        deactivateFilter(oldApp.second.appPermitAnyDns, true);
        deactivateFilter(oldApp.second.splitAppFlowEstablished, true);
        deactivateFilter(oldApp.second.permitApp, true);
        deactivateFilter(oldApp.second.blockAppIpv4, true);
        deactivateFilter(oldApp.second.blockAppIpv6, true);
    }
    apps.clear();
}

void WinDaemon::createBypassAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                       const WfpProviderContextObject &context,
                                       const AppIdKey &appId, bool rewriteDns)
{
    qInfo() << "add bypass app filters:" << appId;
    auto empResult = apps.emplace(appId.copyData(), SplitAppFilters{});
    if(empResult.second)
    {
        auto &appFilters = empResult.first->second;
        activateFilter(appFilters.permitApp, true, AppIdFilter<FWP_IP_VERSION_V4>{appId, 15});
        activateFilter(appFilters.splitAppBind, true,
                        SplitFilter<FWP_IP_VERSION_V4>{appId,
                            PIA_WFP_CALLOUT_BIND_V4,
                            FWPM_LAYER_ALE_BIND_REDIRECT_V4,
                            context,
                            FWP_ACTION_CALLOUT_TERMINATING,
                            15});
        activateFilter(appFilters.splitAppConnect, true,
                        SplitFilter<FWP_IP_VERSION_V4>{appId,
                            PIA_WFP_CALLOUT_CONNECT_V4,
                            FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
                            context,
                            FWP_ACTION_CALLOUT_TERMINATING,
                            15});
        if(rewriteDns)
        {
            // Permit sending to any IPv4 DNS address, then rewrite to the
            // intended address
            activateFilter(appFilters.appPermitAnyDns, true,
                            AppDNSFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>{
                                appId, 15});
            activateFilter(appFilters.splitAppFlowEstablished, true,
                            SplitFilter<FWP_IP_VERSION_V4>{appId,
                                PIA_WFP_CALLOUT_FLOW_ESTABLISHED_V4,
                                FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,
                                context,
                                FWP_ACTION_CALLOUT_INSPECTION,
                                15});
        }
    }
}

void WinDaemon::createOnlyVPNAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                        const WfpProviderContextObject &context,
                                        const AppIdKey &appId, bool rewriteDns)
{
    qInfo() << "add only-VPN app filters:" << appId;
    auto empResult = apps.emplace(appId.copyData(), SplitAppFilters{});
    if(empResult.second)
    {
        auto &appFilters = empResult.first->second;
        // While connected, the normal IPv6 firewall rule should still take care
        // of this, but keep this per-app rule around for robustness.
        activateFilter(appFilters.blockAppIpv6, true,
                       AppIdFilter<FWP_IP_VERSION_V6, FWP_ACTION_BLOCK>{appId, 14});
        activateFilter(appFilters.splitAppBind, true,
                        SplitFilter<FWP_IP_VERSION_V4>{appId,
                            PIA_WFP_CALLOUT_BIND_V4,
                            FWPM_LAYER_ALE_BIND_REDIRECT_V4,
                            context,
                            FWP_ACTION_CALLOUT_TERMINATING,
                            15});
        activateFilter(appFilters.splitAppConnect, true,
                        SplitFilter<FWP_IP_VERSION_V4>{appId,
                            PIA_WFP_CALLOUT_CONNECT_V4,
                            FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
                            context,
                            FWP_ACTION_CALLOUT_TERMINATING,
                            15});
        if(rewriteDns)
        {
            // Permit sending to any IPv4 DNS address, then rewrite to the
            // intended address
            activateFilter(appFilters.appPermitAnyDns, true,
                            AppDNSFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>{
                                appId, 15});
            activateFilter(appFilters.splitAppFlowEstablished, true,
                            SplitFilter<FWP_IP_VERSION_V4>{appId,
                                PIA_WFP_CALLOUT_FLOW_ESTABLISHED_V4,
                                FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,
                                context,
                                FWP_ACTION_CALLOUT_INSPECTION,
                                15});
        }
    }
}

void WinDaemon::createBlockAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                      const AppIdKey &appId)
{
    qInfo() << "add block app filters:" << appId;
    auto empResult = apps.emplace(appId.copyData(), SplitAppFilters{});
    if(empResult.second)
    {
        auto &appFilters = empResult.first->second;
        // Block IPv4, because we can't bind this app to the tunnel (the VPN is
        // not connected).
        activateFilter(appFilters.blockAppIpv4, true,
                       AppIdFilter<FWP_IP_VERSION_V4, FWP_ACTION_BLOCK>{appId, 14});
        // Block IPv6, because the normal IPv6 firewall rule is not active when
        // disconnected (unless the killswitch is set to Always).
        activateFilter(appFilters.blockAppIpv6, true,
                       AppIdFilter<FWP_IP_VERSION_V6, FWP_ACTION_BLOCK>{appId, 14});
    }
}

void WinDaemon::reapplySplitTunnelFirewall(const SplitTunnelFirewallParams &params,
                                           const std::set<const AppIdKey*, PtrValueLess> &newExcludedApps,
                                           const std::set<const AppIdKey*, PtrValueLess> &newVpnOnlyApps)
{
    bool sameExcludedApps = areAppsUnchanged(newExcludedApps, excludedApps);
    bool sameVpnOnlyApps = areAppsUnchanged(newVpnOnlyApps, vpnOnlyApps);

    // If anything changes, we have to delete all filters and recreate
    // everything.  WFP has been known to throw spurious errors if we try to
    // reuse callout or context objects, so we delete everything in order to
    // tear those down and recreate them.
    if(sameExcludedApps && sameVpnOnlyApps && _lastSplitParams == params)
    {
        qInfo() << "Split tunnel rules have not changed - excluded:"
            << excludedApps.size() << "- VPN-only:" << vpnOnlyApps.size()
            << _lastSplitParams;
        return;
    }

    if(_filters.ipInbound != zeroGuid)
    {
        qInfo() << "deactivate IpInbound object";
        deactivateFilter(_filters.ipInbound, true);
    }

    if(_filters.splitCalloutIpInbound != zeroGuid)
    {
       qInfo() << "deactivate IpInbound callout object";
        deactivateFilter(_filters.splitCalloutIpInbound, true);
    }

    if (_filters.ipOutbound != zeroGuid)
    {
        qInfo() << "deactivate IpOutbound object";
        deactivateFilter(_filters.ipOutbound, true);
    }

    if (_filters.splitCalloutIpOutbound != zeroGuid)
    {
        qInfo() << "deactivate IpOutbound callout object";
        deactivateFilter(_filters.splitCalloutIpOutbound, true);
    }

    if (_filters.permitInjectedDns != zeroGuid)
    {
        qInfo() << "deactivate permitInjectedDns filter";
        deactivateFilter(_filters.permitInjectedDns, true);
    }

    if (_filters.splitCalloutConnectAuth != zeroGuid)
    {
        qInfo() << "deactivate ConnectAuth callout object";
        deactivateFilter(_filters.splitCalloutConnectAuth, true);
    }

    // Remove all app filters
    removeSplitTunnelAppFilters(excludedApps, QStringLiteral("excluded"));
    removeSplitTunnelAppFilters(vpnOnlyApps, QStringLiteral("VPN-only"));

    // Delete the old callout and provider context.  WFP does not seem to like
    // reusing the provider context (attempting to reuse it generates an error
    // saying that it does not exist, despite the fact that deleting it
    // succeeds), so out of paranoia we never reuse either object.
    if(_filters.splitCalloutBind != zeroGuid)
    {
        qInfo() << "deactivate bind callout object";
        deactivateFilter(_filters.splitCalloutBind, true);
    }
    if(_filters.splitCalloutConnect != zeroGuid)
    {
        qInfo() << "deactivate connect callout object";
        deactivateFilter(_filters.splitCalloutConnect, true);
    }
    if(_filters.splitCalloutFlowEstablished != zeroGuid)
    {
        qInfo() << "deactivate flow established callout object";
        deactivateFilter(_filters.splitCalloutFlowEstablished, true);
    }
    if(_filters.providerContextKey != zeroGuid)
    {
        qInfo() << "deactivate exclusion provider context object";
        deactivateFilter(_filters.providerContextKey, true);
    }
    if(_filters.vpnOnlyProviderContextKey != zeroGuid)
    {
        qInfo() << "deactivate VPN-only provider context object";
        deactivateFilter(_filters.vpnOnlyProviderContextKey, true);
    }

    // Keep track of the state we used to apply these rules, so we know when to
    // recreate them
    _lastSplitParams = params;
    qInfo() << "Creating split tunnel rules with state" << _lastSplitParams;

    // We can only create exclude rules when the appropriate bind IP address is known
    bool createExcludedRules = !_lastSplitParams._physicalIp.isEmpty() && !newExcludedApps.empty();
    // VPN-only rules are applied even if the last tunnel IP is not known
    // though; we still apply the block rule ("per-app killswitch") until the IP
    // is known.
    bool createVpnOnlyRules = !newVpnOnlyApps.empty();
    // We create bind rules for VPN-only apps when connected and the IP is
    // known; otherwise we just create a block rule (which does not require the
    // callout/context objects).
    bool createVpnOnlyBindRules = _lastSplitParams._hasConnected && !_lastSplitParams._tunnelIp.isEmpty();

    // See Driver.c in desktop-wfp-callout
    struct ContextData
    {
        UINT32 bindIp;
        UINT32 rewriteDnsServer;
        UINT32 dnsSourceIp;
    };

    ContextData bypassContext{}, vpnOnlyContext{};

    bypassContext.bindIp = QHostAddress{_lastSplitParams._physicalIp}.toIPv4Address();
    vpnOnlyContext.bindIp = QHostAddress{_lastSplitParams._tunnelIp}.toIPv4Address();

    // Create the new callout and context objects if any callout rules are
    // needed.
    //
    // These aren't needed if split tunnel is completely inactive, or if we are
    // just blocking VPN-only apps (per-app block rules don't require any
    // callouts).
    if(createExcludedRules || (createVpnOnlyRules && createVpnOnlyBindRules))
    {
        if(_lastSplitParams._forceVpnOnlyDns)
        {
            // The VPN does _not_ have the default route.  Rewrite VPN-only apps
            // to use PIA's configured DNS.
            vpnOnlyContext.rewriteDnsServer = _lastSplitParams._effectiveDnsServers[0];
            // If the DNS address is on loopback, use the loopback interface.
            // Otherwise, use the tunnel device.
            //
            // We could consider using the physical interface if the user
            // entered custom DNS that is on-link for that interface, but
            // currently we always route all DNS through the tunnel (even
            // without split tunnel; the VPN methods always route DNS servers
            // into the tunnel).
            Ipv4Address rewriteDnsServerAddr{vpnOnlyContext.rewriteDnsServer};
            if(rewriteDnsServerAddr.isLoopback())
                vpnOnlyContext.dnsSourceIp = 0x7F000001;    // 127.0.0.1
            // Check if it's on-link for the physical interface (we've already
            // parsed the physical interface address in the bypass context)
            else if(rewriteDnsServerAddr.inSubnet(bypassContext.bindIp, _lastSplitParams._physicalNetPrefix))
                vpnOnlyContext.dnsSourceIp = bypassContext.bindIp;
            else
                vpnOnlyContext.dnsSourceIp = vpnOnlyContext.bindIp;
            qInfo() << "Rewrite DNS for VPN-only apps to"
                << Ipv4Address{vpnOnlyContext.rewriteDnsServer} << "on interface"
                << Ipv4Address{vpnOnlyContext.dnsSourceIp};
        }

        if(_lastSplitParams._forceBypassDns)
        {
            // The VPN has the default route - rewrite DNS for 'bypass' apps.
            // We expect them to send to the PIA-configured DNS; rewrite back to
            // the original DNS.  Do this even if PIA's DNS is the same as the
            // existing DNS, because this still ensures that DNS from bypass
            // apps is sent via the physical interface.
            //
            // We should know the existing DNS servers at this point, but it's
            // possible that there weren't any.  If not, then we'll have to use
            // tunnel DNS for bypass apps (skip this rule).
            if(!_lastSplitParams._existingDnsServers.empty())
            {
                bypassContext.rewriteDnsServer = _lastSplitParams._existingDnsServers[0];
                // Use the loopback interface for a DNS server on loopback, or
                // the physical interface otherwise.  No need to check whether
                // the address is on-link since we're already using the physical
                // interface anyway.
                if(Ipv4Address{bypassContext.rewriteDnsServer}.isLoopback())
                    bypassContext.dnsSourceIp = 0x7F000001;
                else
                    bypassContext.dnsSourceIp = bypassContext.bindIp;
                qInfo() << "Rewrite DNS for bypass apps to"
                    << Ipv4Address{bypassContext.rewriteDnsServer}
                    << "on interface" << Ipv4Address{bypassContext.dnsSourceIp};
            }
            else
            {
                qInfo() << "Existing DNS servers not configured or not know, bypass apps will use tunnel DNS.";
            }
        }

        if(bypassContext.bindIp)
        {
            ProviderContext splitProviderContext{&bypassContext, sizeof(bypassContext)};
            qInfo() << "activate bypass provider context object";
            activateFilter(_filters.providerContextKey, true, splitProviderContext);
        }
        else
            qInfo() << "Not activating bypass provider context object, IP not known";

        if(vpnOnlyContext.bindIp)
        {
            ProviderContext vpnProviderContext{&vpnOnlyContext, sizeof(vpnOnlyContext)};
            qInfo() << "activate VPN-only provider context object";
            activateFilter(_filters.vpnOnlyProviderContextKey, true, vpnProviderContext);
        }
        else
            qInfo() << "Not activating VPN-only provider context object, IP not known";

        qInfo() << "activate callout objects";
        activateFilter(_filters.splitCalloutBind, true, Callout{FWPM_LAYER_ALE_BIND_REDIRECT_V4, PIA_WFP_CALLOUT_BIND_V4});
        activateFilter(_filters.splitCalloutConnect, true, Callout{FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, PIA_WFP_CALLOUT_CONNECT_V4});
        activateFilter(_filters.splitCalloutFlowEstablished, true, Callout{FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4, PIA_WFP_CALLOUT_FLOW_ESTABLISHED_V4});
        activateFilter(_filters.splitCalloutConnectAuth, true, Callout{FWPM_LAYER_ALE_AUTH_CONNECT_V4, PIA_WFP_CALLOUT_CONNECT_AUTH_V4});
        activateFilter(_filters.splitCalloutIpInbound, true, Callout{FWPM_LAYER_INBOUND_IPPACKET_V4, PIA_WFP_CALLOUT_IPPACKET_INBOUND_V4});
        activateFilter(_filters.splitCalloutIpOutbound, true, Callout{FWPM_LAYER_OUTBOUND_IPPACKET_V4, PIA_WFP_CALLOUT_IPPACKET_OUTBOUND_V4});
    }
    else
    {
        qInfo() << "Not creating callout objects; not needed: exclude:"
            << createExcludedRules << "- VPN-only:" << createVpnOnlyRules
            << "- VPN-only bind:" << createVpnOnlyBindRules;
    }

    // If we are rewriting DNS for any app rule, activate these filters and
    // stop the DNSCache service if it's running
    if(bypassContext.rewriteDnsServer || vpnOnlyContext.rewriteDnsServer)
    {
        // If DNS leak protection is active, we need to explicitly permit
        // injected DNS, since the existing DNS servers would be blocked
        // normally.
        if(params._blockDNS)
        {
            activateFilter(_filters.permitInjectedDns, true,
                            CalloutDNSFilter<FWP_IP_VERSION_V4>{
                                PIA_WFP_CALLOUT_CONNECT_AUTH_V4,
                                FWPM_LAYER_ALE_AUTH_CONNECT_V4, 11});
        }
        // DNS rewriting occurs in the IPPACKET layers.  Outbound packets have
        // to be injected at the IP layer (not at the transport layer) because
        // we have to rewrite the source to the physical interface.  That means
        // inbound also has to be handled at the IPPACKET layer because WFP is
        // not aware of the rewritten UDP flows and would otherwise discard
        // these packets.
        activateFilter(_filters.ipInbound, true, IpInboundFilter{PIA_WFP_CALLOUT_IPPACKET_INBOUND_V4, zeroGuid, 10});
        activateFilter(_filters.ipOutbound, true, IpOutboundFilter{PIA_WFP_CALLOUT_IPPACKET_OUTBOUND_V4, zeroGuid, 10});
    }

    if(_lastSplitParams._isConnected && (bypassContext.rewriteDnsServer || vpnOnlyContext.rewriteDnsServer))
    {
        // When connected with split tunnel DNS active, disable Dnscache.  We
        // can't have a system-wide DNS cache since DNS responses may vary by
        // app.  This causes apps to do their own DNS requests, which we can
        // handle on a per-app basis in the callout driver.
        //
        // We have to wait until we're connected to do this (and restore it when
        // not connected) because Dnscache must be up to apply DNS servers
        // statically - which we do when connecting with WireGuard or the
        // OpenVPN static method.
        _dnsCacheControl.disableDnsCache();
    }
    else
    {
        // Restore Dnscache.  We need Dnscache to be up in order to connect with
        // WireGuard or the OpenVPN static (non-DHCP) method.
        _dnsCacheControl.restoreDnsCache();
    }

    if(createExcludedRules)
    {
        qInfo() << "Creating exclude rules for" << newExcludedApps.size() << "apps";
        for(auto &pAppId : newExcludedApps)
        {
            createBypassAppFilters(excludedApps, _filters.providerContextKey,
                                   *pAppId,
                                   bypassContext.rewriteDnsServer && *pAppId != _unboundAppId);
        }
    }

    if(createVpnOnlyRules)
    {
        qInfo() << "Creating VPN-only rules for" << newExcludedApps.size() << "apps";
        for(auto &pAppId : newVpnOnlyApps)
        {
            if(createVpnOnlyBindRules)
            {
                createOnlyVPNAppFilters(vpnOnlyApps, _filters.vpnOnlyProviderContextKey,
                                        *pAppId,
                                        vpnOnlyContext.rewriteDnsServer && *pAppId != _unboundAppId);
            }
            else
            {
                createBlockAppFilters(vpnOnlyApps, *pAppId);
            }
        }
    }
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

        const auto &appId = pAppId->printableString();
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

        const auto &appId = pAppId->printableString();
        auto title = QStringLiteral("WFP events (VPN-only %1 - %2)").arg(i).arg(appId);
        auto cmdParams = QStringLiteral("wfp show netevents appid = \"%1\" file = -").arg(appId);
        file.writeCommand(title, "netsh", cmdParams);
        ++i;
    }

    // Wireguard logs
    file.writeCommand("WireGuard Logs", Path::WireguardServiceExecutable,
                      QStringList{QStringLiteral("/dumplog"), Path::ConfigLogFile});

    // Installed and running drivers (buggy drivers may prevent TAP installation)
    file.writeCommand("Drivers", QStringLiteral("driverquery"), {QStringLiteral("/v")});

    // DNS
    file.writeCommand("Resolve-DnsName (www.pia.com)", "powershell.exe", QStringLiteral("/C Resolve-DnsName www.privateinternetaccess.com"));
    file.writeCommand("Resolve-DnsName (-Server piadns www.pia.com)", "powershell.exe", QStringLiteral("/C Resolve-DnsName www.privateinternetaccess.com -Server %1").arg(piaLegacyDnsPrimary()));
    file.writeCommand("ping (ping www.pia.com)", "ping", QStringLiteral("www.privateinternetaccess.com /w 1000 /n 1"));
    file.writeCommand("ping (ping 209.222.18.222)", "ping", QStringLiteral("%1 /w 1000 /n 1").arg(piaLegacyDnsPrimary()));
}

void WinDaemon::checkWintunInstallation()
{
    const auto &installedProducts = findInstalledWintunProducts();
    qInfo() << "WinTUN installed products:" << installedProducts.size();
    for(const auto &product : installedProducts)
        qInfo() << "-" << product;
    _state.wintunMissing(installedProducts.empty());
}

class TraceMemSize : public DebugTraceable<TraceMemSize>
{
public:
    TraceMemSize(std::size_t mem) : _mem{mem} {}

public:
    void trace(QDebug &dbg) const
    {
        const std::array<QStringView, 3> units
        {{
            {L"B"},
            {L"KiB"},
            {L"MiB"}
        }};

        int unitIdx{0};
        std::size_t memInUnits{_mem};
        while(unitIdx+1 < units.size() && memInUnits > 1024)
        {
            ++unitIdx;
            memInUnits /= 1024;
        }

        QDebugStateSaver save{dbg};
        dbg.noquote() << memInUnits << units[unitIdx];
    }

private:
    std::size_t _mem;
};

void WinDaemon::traceClientMemory()
{
    WinHandle procSnapshot{::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};

    PROCESSENTRY32W proc{};
    proc.dwSize = sizeof(proc);

    QStringView clientModule{BRAND_CODE L"-client.exe"};
    QStringView cliModule{BRAND_CODE L"ctl.exe"};

    int clientProcesses{0};

    BOOL nextProc = ::Process32FirstW(procSnapshot.get(), &proc);
    while(nextProc)
    {
        // Avoid wchar_t[] constructor for QStringView which assumes the string
        // fills the entire array
        QStringView processName{&proc.szExeFile[0]};
        if(clientModule == processName || cliModule == processName)
        {
            ++clientProcesses;
            WinHandle clientProcess{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                                                  false, proc.th32ProcessID)};
            DWORD dwError{ERROR_SUCCESS};
            if(!clientProcess)
                dwError = ::GetLastError();
            PROCESS_MEMORY_COUNTERS_EX procMem{};
            procMem.cb = sizeof(procMem);
            BOOL gotMemory = FALSE;
            if(clientProcess)
            {
                gotMemory = ::GetProcessMemoryInfo(clientProcess.get(),
                                                   reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&procMem),
                                                   procMem.cb);
                if(!gotMemory)
                    dwError = ::GetLastError();
            }

            if(gotMemory)
            {
                qInfo() << "PID" << proc.th32ProcessID
                    << processName
                    << "- Private mem:" << TraceMemSize{procMem.PrivateUsage}
                    << "- Nonpaged pool:" << TraceMemSize{procMem.QuotaNonPagedPoolUsage}
                    << "- Paged pool:" << TraceMemSize{procMem.QuotaPagedPoolUsage}
                    << "- Working set:" << TraceMemSize{procMem.WorkingSetSize};
            }
            else
            {
                qInfo() << "PID" << proc.th32ProcessID
                    << QStringView{proc.szExeFile}
                    << "- can't access memory usage -"
                    << WinErrTracer{dwError};
            }
        }
        nextProc = ::Process32NextW(procSnapshot.get(), &proc);
    }

    DWORD dwError = ::GetLastError();
    if(dwError != ERROR_SUCCESS && dwError != ERROR_NO_MORE_FILES)
    {
        qWarning() << "Unable to enumerate processes:" << WinErrTracer{dwError};
    }

    qInfo() << "Found" << clientProcesses << "running client processes";
}

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
