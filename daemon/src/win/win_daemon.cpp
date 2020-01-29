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
#include "path.h"
#include "win.h"
#include "../../extras/installer/win/tap.inl"

// The 'bind' callout GUID is the GUID used in 1.7 and earlier; the WFP callout
// only handled the bind layer in those releases.
GUID PIA_WFP_CALLOUT_BIND_V4 = {0xb16b0a6e, 0x2b2a, 0x41a3, { 0x8b, 0x39, 0xbd, 0x3f, 0xfc, 0x85, 0x5f, 0xf8 } };
GUID PIA_WFP_CALLOUT_CONNECT_V4 = { 0xb80ca14a, 0xa807, 0x4ef2, { 0x87, 0x2d, 0x4b, 0x1a, 0x51, 0x82, 0x54, 0x2 } };

void WinNetworkAdapter::setMetricToLowest()
 {
    if (!luid) return;

    qDebug() << "Attempting to set metric to a low value!";
    auto currentMetricIPv4 = getMetric(AF_INET);
    auto currentMetricIPv6 = getMetric(AF_INET6);

    if (currentMetricIPv4 != interfaceMetric && currentMetricIPv4 != -1) {
       _savedMetricValueIPv4 = currentMetricIPv4;
       setMetric(AF_INET, interfaceMetric);
    }

    if (currentMetricIPv6 != interfaceMetric && currentMetricIPv6 != -1) {
        _savedMetricValueIPv6 = currentMetricIPv6;
        setMetric(AF_INET6, interfaceMetric);
    }
}

void WinNetworkAdapter::restoreOriginalMetric()
{
    if (!luid) return;

    qDebug() << "Attempting to restore saved interface metric!";

    if (_savedMetricValueIPv4 > -1) {
        setMetric(AF_INET, _savedMetricValueIPv4);
        _savedMetricValueIPv4 = -1;
    }

    if (_savedMetricValueIPv6 > -1) {
        setMetric(AF_INET6, _savedMetricValueIPv6);
        _savedMetricValueIPv6 = -1;
    }
}

void WinNetworkAdapter::setMetric(const ADDRESS_FAMILY family, ULONG metric)
{
    DWORD err = 0;
    MIB_IPINTERFACE_ROW ipiface;
    InitializeIpInterfaceEntry(&ipiface);
    ipiface.Family = family;
    ipiface.InterfaceLuid.Value = luid;
    err = GetIpInterfaceEntry(&ipiface);

    if (err != NO_ERROR) {
        qError() << "Error retrieving interface with GetIpInterfaceEntry: code:" << err;
        return;
    }

    if (family == AF_INET) {
        /* required for IPv4 as per MSDN */
        ipiface.SitePrefixLength = 0;
    }

    ipiface.Metric = metric;

    if (metric == 0) {
       ipiface.UseAutomaticMetric = TRUE;
    } else {
       ipiface.UseAutomaticMetric = FALSE;
    }

    err = SetIpInterfaceEntry(&ipiface);

    if (err != NO_ERROR) {
        qError() << "Error setting interface metric with SetIpInterfaceEntry: code:" << err;
        return;
    }
}

int WinNetworkAdapter::getMetric(const ADDRESS_FAMILY family)
{
    DWORD err = 0;
    MIB_IPINTERFACE_ROW ipiface;
    InitializeIpInterfaceEntry(&ipiface);
    ipiface.Family = family;
    ipiface.InterfaceLuid.Value = luid;
    err = GetIpInterfaceEntry(&ipiface);
    if (err != NO_ERROR) {
        qError() << "Error retrieving interface with GetIpInterfaceEntry: code:" << err;
        return -1;
    }
    if (ipiface.UseAutomaticMetric) {
        return 0;
    } else {
       return (int)ipiface.Metric;
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

void WINAPI WinDaemon::ipChangeCallback(PVOID callerContext,
                                        PMIB_IPINTERFACE_ROW pRow,
                                        MIB_NOTIFICATION_TYPE notificationType)
{
    // Figure out if this change means that we should re-check whether the TAP
    // adapter still exists.
    // We re-check it for any add or delete notification.  In principle we could
    // attempt to figure out whether it actually affects an adapter that we care
    // about, but this is straightforward enough and adapter adds/deletes are
    // not common.
    // Note that pRow only has a few fields filled in; the doc explains how you
    // would use it to actually get a full MIB_IPINTERFACE_ROW:
    // https://docs.microsoft.com/en-us/windows/desktop/api/netioapi/nf-netioapi-notifyipinterfacechange
    Q_UNUSED(pRow);
    if(notificationType == MibAddInstance ||
        notificationType == MibDeleteInstance ||
        notificationType == MibInitialNotification)
    {
        // This callback occurs on a worker thread specifically for this
        // purpose.  Queue a call over to WinDaemon on the service thread.
        WinDaemon *pThis = reinterpret_cast<WinDaemon*>(callerContext);
        Q_ASSERT(pThis);    // Ensured by WinDaemon ctor
        QMetaObject::invokeMethod(pThis, &WinDaemon::checkNetworkAdapter,
                                  Qt::QueuedConnection);
    }
}

WinDaemon::WinDaemon(const QStringList& arguments, QObject* parent)
    : Daemon(arguments, parent)
    , MessageWnd(WindowType::Invisible)
    , _firewall(new FirewallEngine(this))
    , _hnsdAppId{nullptr, Path::HnsdExecutable}
    , _lastConnected{false}
    , _ipNotificationHandle(nullptr)
    , _wfpCalloutMonitor{L"PiaWfpCallout"}
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

    auto notifyResult = ::NotifyIpInterfaceChange(AF_UNSPEC, &ipChangeCallback,
                                                  reinterpret_cast<void*>(this),
                                                  TRUE, &_ipNotificationHandle);
    if(notifyResult != NO_ERROR)
    {
        qWarning() << "Couldn't enable interface change notifications:" << notifyResult;
    }

    connect(&_wfpCalloutMonitor, &ServiceMonitor::serviceStateChanged, this,
            [this](DaemonState::NetExtensionState extState)
            {
                state().netExtensionState(qEnumToString(extState));
            });
    state().netExtensionState(qEnumToString(_wfpCalloutMonitor.lastState()));
    qInfo() << "Initial callout driver state:" << state().netExtensionState();

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
}

WinDaemon::WinDaemon(QObject* parent)
    : WinDaemon(QCoreApplication::arguments(), parent)
{

}

WinDaemon::~WinDaemon()
{
    if (_firewall)
    {
        qInfo() << "Cleaning up WFP objects";
        _firewall->removeAll();
        _firewall->uninstallProvider();
        _firewall->checkLeakedObjects();
    }
    else
        qInfo() << "Firewall was not initialized, nothing to clean up";

    if(_ipNotificationHandle)
    {
        qInfo() << "Closing IP interface change callback";
        // Cancel the IP interface change callback.  This ends the thread that
        // is used to call the callback (and ensures we don't get a callback
        // with a 'this' that's no longer valid).
        ::CancelMibChangeNotify2(_ipNotificationHandle);
    }
    else
        qInfo() << "IP interface change callback was not initialized, nothing to clean up";

    qInfo() << "WinDaemon shutdown complete";
}

QSharedPointer<NetworkAdapter> WinDaemon::getNetworkAdapter()
{
    // For robustness, when making a connection, we always re-query for the
    // network adapter, in case the change notifications aren't 100% reliable.
    // Also update the DaemonState accordingly to keep everything in sync.
    auto adapters = getAllNetworkAdapters();
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

QList<QSharedPointer<WinNetworkAdapter>> WinDaemon::getAllNetworkAdapters()
{
    QList<QSharedPointer<WinNetworkAdapter>> adapters;

    int iterations = 0;

    ULONG error;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_WINS_INFO | GAA_FLAG_INCLUDE_GATEWAYS;
    ULONG size = 15000;

    IP_ADAPTER_ADDRESSES* addresses = NULL;

    do
    {
        if (NULL == (addresses = (IP_ADAPTER_ADDRESSES*)malloc(size)))
            throw SystemError(HERE, ERROR_NOT_ENOUGH_MEMORY);

        if ((error = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, addresses, &size)) != ERROR_BUFFER_OVERFLOW)
            break;

        free(addresses);
        addresses = NULL;

    } while (++iterations < 3);

    if (error != ERROR_SUCCESS)
        throw SystemError(HERE, error);

    for (auto address = addresses; address; address = address->Next)
    {
        if (address->Description && wcsstr(address->Description, L"Private Internet Access Network Adapter") != NULL)
        {
            auto adapter = QSharedPointer<WinNetworkAdapter>::create(address->AdapterName);
            adapter->luid = address->Luid.Value;
            adapter->ifIndex = address->IfIndex;
            adapter->adapterName = QString::fromWCharArray(address->Description);
            adapter->connectionName = QString::fromWCharArray(address->FriendlyName);
            adapter->isCustomTap = true;
            adapters.append(std::move(adapter));
        }
    }
    free(addresses);
    return adapters;
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

    QString dnsServers[2] { params.dnsServers.value(0), params.dnsServers.value(1) }; // default-constructed if missing
    auto networkAdapter = params.adapter.staticCast<WinNetworkAdapter>();

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

    // Exempt traffic going over the TAP adapter used by OpenVPN.
    UINT64 luid = networkAdapter ? networkAdapter->luid : 0;
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

    // Add rules to block non-PIA DNS servers if connected and DNS leak protection is enabled
    logFilter("blockDNS", _filters.blockDNS, params.blockDNS);
    updateBooleanFilter(blockDNS[0], params.blockDNS, DNSFilter<FWP_ACTION_BLOCK, FWP_IP_VERSION_V4>(10));
    updateBooleanFilter(blockDNS[1], params.blockDNS, DNSFilter<FWP_ACTION_BLOCK, FWP_IP_VERSION_V6>(10));
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

    // Handshake related filters
    // (1) First we block everything coming from the handshake process
    logFilter("allowHnsd (block everything)", _filters.blockHnsd, luid && params.allowHnsd, luid != _filterAdapterLuid);
    updateBooleanInvalidateFilter(blockHnsd[0], luid && params.allowHnsd, luid != _filterAdapterLuid, ApplicationFilter<FWP_ACTION_BLOCK, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::HnsdExecutable, 14));

    // (2) Next we poke a hole in this block but only allow data that goes across the tunnel
    logFilter("allowHnsd (tunnel traffic)", _filters.permitHnsd, luid && params.allowHnsd, luid != _filterAdapterLuid);
    updateBooleanInvalidateFilter(permitHnsd[0], luid && params.allowHnsd, luid != _filterAdapterLuid, ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(Path::HnsdExecutable, 15,
        Condition<FWP_UINT64>{FWPM_CONDITION_IP_LOCAL_INTERFACE, FWP_MATCH_EQUAL, &luid},

        // OR'ing of conditions is done automatically when you have 2 or more consecutive conditions of the same fieldId.
        Condition<FWP_UINT16>{FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 53},

        // 13038 is the Handshake control port
        Condition<FWP_UINT16>{FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 13038}
    ));

    // Always permit loopback traffic, including IPv6.
    logFilter("allowLoopback", _filters.permitLocalhost, params.allowLoopback);
    updateBooleanFilter(permitLocalhost[0], params.allowLoopback, LocalhostFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(15));
    updateBooleanFilter(permitLocalhost[1], params.allowLoopback, LocalhostFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(15));

    // Get the current set of excluded app IDs.  If they've changed we recreate
    // all app rules, but if they stay the same we don't recreate them.
    std::set<const AppIdKey*, PtrValueLess> newExcludedApps, newVpnOnlyApps;
    if(params.enableSplitTunnel)
    {
        newExcludedApps = _appMonitor.getExcludedAppIds();
        newVpnOnlyApps = _appMonitor.getVpnOnlyAppIds();

        if(!params.defaultRoute)
        {
            // Put hnsd in the VPN since we did not use the VPN as the default
            // route
            newVpnOnlyApps.insert(&_hnsdAppId);
        }
    }

    qInfo() << "Number of excluded apps" << newExcludedApps.size();
    qInfo() << "Number of vpnOnly apps" << newVpnOnlyApps.size();

    reapplySplitTunnelFirewall(params.splitTunnelNetScan.ipAddress(),
                               _state.tunnelDeviceLocalAddress(),
                               newExcludedApps, newVpnOnlyApps, params.hasConnected);

    tx.commit();
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
        deactivateFilter(oldApp.second.permitApp, true);
        deactivateFilter(oldApp.second.blockAppIpv4, true);
        deactivateFilter(oldApp.second.blockAppIpv6, true);
    }
    apps.clear();
}

void WinDaemon::createBypassAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                       const WfpProviderContextObject &context,
                                       const AppIdKey &appId)
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
                            15});
        activateFilter(appFilters.splitAppConnect, true,
                        SplitFilter<FWP_IP_VERSION_V4>{appId,
                            PIA_WFP_CALLOUT_CONNECT_V4,
                            FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
                            context,
                            15});
    }
}

void WinDaemon::createOnlyVPNAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                        const WfpProviderContextObject &context,
                                        const AppIdKey &appId)
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
                            15});
        activateFilter(appFilters.splitAppConnect, true,
                        SplitFilter<FWP_IP_VERSION_V4>{appId,
                            PIA_WFP_CALLOUT_CONNECT_V4,
                            FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
                            context,
                            15});
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

void WinDaemon::reapplySplitTunnelFirewall(const QString &newSplitTunnelIp,
                                           const QString &newTunnelIp,
                                           const std::set<const AppIdKey*, PtrValueLess> &newExcludedApps,
                                           const std::set<const AppIdKey*, PtrValueLess> &newVpnOnlyApps,
                                           bool hasConnected)
{
    bool sameExcludedApps = areAppsUnchanged(newExcludedApps, excludedApps);
    bool sameVpnOnlyApps = areAppsUnchanged(newVpnOnlyApps, vpnOnlyApps);

    // If anything changes, we have to delete all filters and recreate
    // everything.  WFP has been known to throw spurious errors if we try to
    // reuse callout or context objects, so we delete everything in order to
    // tear those down and recreate them.
    if(sameExcludedApps && sameVpnOnlyApps && _lastSplitTunnelIp == newSplitTunnelIp &&
        _lastTunnelIp == newTunnelIp && _lastConnected == hasConnected)
    {
        qInfo() << "Split tunnel rules have not changed - excluded:"
            << excludedApps.size() << "- VPN-only:" << vpnOnlyApps.size()
            << "split tunnel IP known:" << !_lastSplitTunnelIp.isEmpty()
            << "tunnel IP known:" << !_lastTunnelIp.isEmpty()
            << "and have connected:" << _lastConnected;
        return;
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
    if(_filters.vpnOnlyProviderContextKey != zeroGuid)
    {
        qInfo() << "deactivate provider context object";
        deactivateFilter(_filters.vpnOnlyProviderContextKey, true);
    }

    // Keep track of the state we used to apply these rules, so we know when to
    // recreate them
    _lastSplitTunnelIp = newSplitTunnelIp;
    _lastTunnelIp = newTunnelIp;
    _lastConnected = hasConnected;
    qInfo() << "Creating split tunnel rules with state - split tunnel IP known:"
        << !_lastSplitTunnelIp.isEmpty() << "- tunnel IP known:"
        << !_lastTunnelIp.isEmpty() << "- have connected:" << _lastConnected;

    // We can only create exclude rules when the appropriate bind IP address is known
    bool createExcludedRules = !_lastSplitTunnelIp.isEmpty() && !newExcludedApps.empty();
    // VPN-only rules are applied even if the last tunnel IP is not known
    // though; we still apply the block rule ("per-app killswitch") until the IP
    // is known.
    bool createVpnOnlyRules = !newVpnOnlyApps.empty();
    // We create bind rules for VPN-only apps when connected and the IP is
    // known; otherwise we just create a block rule (which does not require the
    // callout/context objects).
    bool createVpnOnlyBindRules = _lastConnected && !_lastTunnelIp.isEmpty();

    // Create the new callout and context objects if any rules are needed
    if(createExcludedRules || (createVpnOnlyRules && createVpnOnlyBindRules))
    {
        UINT32 splitIpAddress = QHostAddress{_lastSplitTunnelIp}.toIPv4Address();
        if(splitIpAddress)
        {
            ProviderContext splitProviderContext{&splitIpAddress, sizeof(UINT32)};
            qInfo() << "activate exclusion provider context object";
            activateFilter(_filters.providerContextKey, true, splitProviderContext);
        }
        else
            qInfo() << "Not activating exclusion provider context object, IP not known";

        UINT32 vpnIpAddress = QHostAddress{_lastTunnelIp}.toIPv4Address();
        if(vpnIpAddress)
        {
            ProviderContext vpnProviderContext{&vpnIpAddress, sizeof(UINT32)};
            qInfo() << "activate VPN-only provider context object";
            activateFilter(_filters.vpnOnlyProviderContextKey, true, vpnProviderContext);
        }
        else
            qInfo() << "Not activating VPN-only provider context object, IP not known";

        qInfo() << "activate callout objects";
        activateFilter(_filters.splitCalloutBind, true, Callout{FWPM_LAYER_ALE_BIND_REDIRECT_V4, PIA_WFP_CALLOUT_BIND_V4});
        activateFilter(_filters.splitCalloutConnect, true, Callout{FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, PIA_WFP_CALLOUT_CONNECT_V4});
    }
    else
    {
        qInfo() << "Not creating callout objects; not needed: exclude:"
            << createExcludedRules << "- VPN-only:" << createVpnOnlyRules
            << "- VPN-only bind:" << createVpnOnlyBindRules;
    }

    if(createExcludedRules)
    {
        qInfo() << "Creating exclude rules for" << newExcludedApps.size() << "apps";
        for(auto &pAppId : newExcludedApps)
        {
            createBypassAppFilters(excludedApps, _filters.providerContextKey,
                                   *pAppId);
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
                                        *pAppId);
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

void WinDaemon::RPC_checkCalloutState()
{
    _wfpCalloutMonitor.doManualCheck();
}

void WinDaemon::writePlatformDiagnostics(DiagnosticsFile &file)
{
    file.writeCommand("OS Version", "wmic", QStringLiteral("os get Caption,CSDVersion,BuildNumber,Version /value"));
    file.writeCommand("Interfaces (ipconfig)", "ipconfig", QStringLiteral("/all"));
    file.writeCommand("Routes (netstat -nr)", "netstat", QStringLiteral("-nr"));

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
        auto title = QStringLiteral("WFP events (%1 - %2)").arg(i).arg(appId);
        auto cmdParams = QStringLiteral("wfp show netevents appid = \"%1\" file = -").arg(appId);
        file.writeCommand(title, "netsh", cmdParams);
        ++i;
    }
}
