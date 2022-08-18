// Copyright (c) 2022 Private Internet Access, Inc.
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
#include "win_networks.h"
#include "win_nativewifi.h"
#include "win_servicestate.h"
#include "win.h"
#include <common/src/win/win_util.h>
#include <QMetaObject>
#include <iphlpapi.h>

namespace
{
    class WinMibDeleter
    {
    public:
        template<class MibType>
        void operator()(MibType *pMib){::FreeMibTable(reinterpret_cast<void*>(pMib));}
    };

    template<class MibType>
    using WinMibPtr = WinGenericHandle<MibType*, WinMibDeleter>;

    QHostAddress parseWinSockaddr(const SOCKADDR_INET &addr)
    {
        return QHostAddress{reinterpret_cast<const sockaddr*>(&addr)};
    }
}

class WinNetworks : public NetworkMonitor
{
private:
    static QString changeTraceName(MIB_NOTIFICATION_TYPE notificationType);
    static void queueUpdate(PVOID callerContext);

    // Callback used to receive route change notifications on a thread created
    // by IPHelper.
    static void WINAPI routeChangeCallback(PVOID callerContext,
                                           PMIB_IPFORWARD_ROW2 pRow,
                                           MIB_NOTIFICATION_TYPE notificationType);
    static void WINAPI unicastIpChangeCallback(PVOID callerContext,
                                               PMIB_UNICASTIPADDRESS_ROW pRow,
                                               MIB_NOTIFICATION_TYPE notificationType);
    static void WINAPI ipInterfaceChangeCallback(PVOID callerContext,
                                                 PMIB_IPINTERFACE_ROW pRow,
                                                 MIB_NOTIFICATION_TYPE notificationType);

public:
    WinNetworks();
    ~WinNetworks();

private:
    // Read the routing table and report the default IPv4 and IPv6 interfaces.
    std::vector<NetworkConnection> readRoutes();
    // Read the routing table, then emit the new network connections
    void updateConnections();
    // Create WinNativeWifi, trace if it can't connect
    void connectNativeWifi();
    // Handle a change in the state of WlanSvc - can connect or disconnect the
    // Native Wifi client.  We don't care about the PID.
    void onWlanSvcStateChange(WinServiceState::State newState);

private:
    // State of the WLAN AutoConfig ('WlanSvc') service.  We can only connect to
    // the WLAN client when this is up.
    WinServiceState _wlanSvcState;
    // Our Native Wifi client that reads state from WlanSvc.  This is only
    // present when it's possible to connect to the service.
    nullable_t<WinNativeWifi> _pWifi;
    HANDLE _routeNotificationHandle, _unicastIpNotificationHandle,
           _ipInterfaceNotificationHandle;
};

QString WinNetworks::changeTraceName(MIB_NOTIFICATION_TYPE notificationType)
{
    switch(notificationType)
    {
        default:
            return QString::number(notificationType);
        case MibInitialNotification:
            return QStringLiteral("initial");
        case MibAddInstance:
            return QStringLiteral("add");
        case MibDeleteInstance:
            return QStringLiteral("delete");
        case MibParameterNotification:
            return QStringLiteral("modify");
    }
}

void WinNetworks::queueUpdate(PVOID callerContext)
{
    WinNetworks *pThis = reinterpret_cast<WinNetworks*>(callerContext);
    Q_ASSERT(pThis);    // Ensured by ctor
    QMetaObject::invokeMethod(pThis, &WinNetworks::updateConnections,
                              Qt::ConnectionType::QueuedConnection);
}

void WINAPI WinNetworks::routeChangeCallback(PVOID callerContext,
                                             PMIB_IPFORWARD_ROW2 pRow,
                                             MIB_NOTIFICATION_TYPE notificationType)
{
    // For any change, we just re-scan the routing table rather than trying to
    // figure out if the change could have impacted the default networks we
    // detected.  Trace the change though for diagnostics.
    //
    // We don't request an initial notification, so pRow should always be set
    // according to doc, but check it.
    if(pRow)
    {
        auto destination = parseWinSockaddr(pRow->DestinationPrefix.Prefix);
        auto nextHop = parseWinSockaddr(pRow->NextHop);
        qInfo() << "Route change:" << changeTraceName(notificationType) << "-"
            << destination.toString() << "/" << pRow->DestinationPrefix.PrefixLength
            << "->" << nextHop.toString();
    }

    queueUpdate(callerContext);
}

void WINAPI WinNetworks::unicastIpChangeCallback(PVOID callerContext,
                                                 PMIB_UNICASTIPADDRESS_ROW pRow,
                                                 MIB_NOTIFICATION_TYPE notificationType)
{
    if(pRow)
    {
        QHostAddress addr = parseWinSockaddr(pRow->Address);
        qInfo() << "Unicast IP change:" << changeTraceName(notificationType)
            << "-" << addr.toString() << "on" << pRow->InterfaceLuid.Value
            << "=" << pRow->InterfaceIndex;
    }

    queueUpdate(callerContext);
}

void WINAPI WinNetworks::ipInterfaceChangeCallback(PVOID callerContext,
                                                   PMIB_IPINTERFACE_ROW pRow,
                                                   MIB_NOTIFICATION_TYPE notificationType)
{
    if(pRow)
    {
        qInfo() << "Interface change:" << changeTraceName(notificationType)
            << pRow->InterfaceLuid.Value << "=" << pRow->InterfaceIndex
            << "connected:" << pRow->Connected << "metric:" << pRow->Metric;
    }

    queueUpdate(callerContext);
}


WinNetworks::WinNetworks()
    : _wlanSvcState{L"WlanSvc", 0}, // We don't need any start/stop rights
      _routeNotificationHandle{}, _unicastIpNotificationHandle{},
      _ipInterfaceNotificationHandle{}
{
    connect(&_wlanSvcState, &WinServiceState::stateChanged, this,
            &WinNetworks::onWlanSvcStateChange);
    // Although the service could already be up, WinServiceMonitor reports it
    // asynchronously.  We don't need to try to connect now.
    // Postcondition of WinServiceState::WinServiceState()
    Q_ASSERT(_wlanSvcState.lastState() != WinServiceState::State::Running);

    // We don't need initial callbacks for any of these notifications - we just
    // scan the routing table once after initializing
    auto notifyResult = ::NotifyRouteChange2(AF_UNSPEC, &WinNetworks::routeChangeCallback,
                                             reinterpret_cast<void*>(this),
                                             false, &_routeNotificationHandle);
    if(notifyResult != NO_ERROR)
    {
        qWarning() << "Couldn't enable route change notification:" << notifyResult;
    }

    notifyResult = ::NotifyUnicastIpAddressChange(AF_UNSPEC, &WinNetworks::unicastIpChangeCallback,
                                                  reinterpret_cast<void*>(this),
                                                  false, &_unicastIpNotificationHandle);
    if(notifyResult != NO_ERROR)
    {
        qWarning() << "Couldn't enable unicast IP address change notifications:" << notifyResult;
    }

    notifyResult = ::NotifyIpInterfaceChange(AF_UNSPEC, &WinNetworks::ipInterfaceChangeCallback,
                                             reinterpret_cast<void*>(this),
                                             false, &_ipInterfaceNotificationHandle);
    if(notifyResult != NO_ERROR)
    {
        qWarning() << "Couldn't enable IP interface change notifications:" << notifyResult;
    }

    updateConnections();
}

WinNetworks::~WinNetworks()
{
    if(_routeNotificationHandle)
        ::CancelMibChangeNotify2(_routeNotificationHandle);
}

std::vector<NetworkConnection> WinNetworks::readRoutes()
{
    // Unlike Mac and Linux, Windows actually does offer a GetBestRoute2() API
    // which can be used to inspect the routing table.
    //
    // However, we're not using it here, we fetch the whole routing table and
    // specifically look for default gateway routes.  This is more robust when
    // we override the default gateway for different connection methods.
    //
    // - For OpenVPN, if we specifically look for the route to the VPN server,
    //   it should still find the correct gateway, because OpenVPN adds a route
    //   for that server pointing it back to the default gateway.  However, we
    //   could observe transients while routes are being set up, and that method
    //   requires us to know the current VPN server IP.
    // - For WireGuard, this doesn't work while connected - WG redirects the
    //   default gateway but does not create an override route for the VPN
    //   server - it works differently to permit roaming.

    // Get all interfaces.  We need the interfaces for their interface metrics
    // and to know whether the interface is actually connected (routes hang
    // around after an interface is disconnected, but netstat -nr appears to
    // hide them).
    WinMibPtr<MIB_IPINTERFACE_TABLE> pItfTable;
    auto tableResult = ::GetIpInterfaceTable(AF_UNSPEC, pItfTable.receive());
    if(tableResult != NO_ERROR || !pItfTable)
    {
        qWarning() << "Unable to get interfaces to read routing table -" << tableResult;
        throw Error{HERE, Error::Code::Unknown};
    }

    // The local IP interface table is needed to find the local IP addresses
    // once we find the default routes.
    WinMibPtr<MIB_UNICASTIPADDRESS_TABLE> pAddrTable;
    tableResult = ::GetUnicastIpAddressTable(AF_UNSPEC, pAddrTable.receive());
    if(tableResult != NO_ERROR || !pAddrTable)
    {
        qWarning() << "Unable to get address table to read routing table -" << tableResult;
        throw Error{HERE, Error::Code::Unknown};
    }

    // The route table is used to find gateway routes.
    WinMibPtr<MIB_IPFORWARD_TABLE2> pRouteTable;
    tableResult = ::GetIpForwardTable2(AF_UNSPEC, pRouteTable.receive());
    if(tableResult != NO_ERROR || !pRouteTable)
    {
        qWarning() << "Unable to read routing table -" << tableResult;
        throw Error{HERE, Error::Code::Unknown};
    }

    // Collate all of this information by LUID - create entries for each known
    // interface, then add addresses, gateways, default IPv4/6, and Wi-Fi state.
    struct InterfaceInfo
    {
        // IPv4 and IPv6 interface rows - the Connected flag and Metric are
        // needed when examining gateway routes.  Either could be nullptr
        const MIB_IPINTERFACE_ROW *pItf4;
        const MIB_IPINTERFACE_ROW *pItf6;
        kapps::core::Ipv4Address gatewayIpv4;
        kapps::core::Ipv6Address gatewayIpv6;
        std::vector<std::pair<kapps::core::Ipv4Address, unsigned>> addressesIpv4;
        std::vector<std::pair<kapps::core::Ipv6Address, unsigned>> addressesIpv6;
    };
    std::unordered_map<WinLuid, InterfaceInfo> interfacesByLuid;
    interfacesByLuid.reserve(pItfTable.get()->NumEntries);
    for(unsigned long i=0; i<pItfTable.get()->NumEntries; ++i)
    {
        const MIB_IPINTERFACE_ROW &row = pItfTable.get()->Table[i];
        // Create this interface's entry and store the reference to the row
        InterfaceInfo &info = interfacesByLuid[row.InterfaceLuid];
        if(row.Family == AF_INET)
            info.pItf4 = &row;
        else if(row.Family == AF_INET6)
            info.pItf6 = &row;
    }

    auto getItfForLuid = [&](const WinLuid &luid) -> InterfaceInfo *
    {
        auto itItf = interfacesByLuid.find(luid);
        if(itItf == interfacesByLuid.end())
            return nullptr;
        return &itItf->second;
    };

    // Go through the address table and apply addresses
    for(unsigned long i=0; i<pAddrTable.get()->NumEntries; ++i)
    {
        const auto &addr = pAddrTable.get()->Table[i];
        InterfaceInfo *pItf = getItfForLuid(addr.InterfaceLuid);
        if(!pItf)
            continue;   // Ignore address for unknown interface

        if(addr.Address.si_family == AF_INET)
        {
            pItf->addressesIpv4.push_back({kapps::core::Ipv4Address{ntohl(addr.Address.Ipv4.sin_addr.S_un.S_addr)},
                                           addr.OnLinkPrefixLength});
        }
        else if(addr.Address.si_family == AF_INET6)
        {
            pItf->addressesIpv6.push_back({kapps::core::Ipv6Address{addr.Address.Ipv6.sin6_addr.u.Byte},
                                           addr.OnLinkPrefixLength});
            // Ignore link-local
            if(pItf->addressesIpv6.back().first.isLinkLocal())
                pItf->addressesIpv6.pop_back();
        }
    }

    // Go through the route table and apply gateways.  Keep track of which one
    // is the best.  If an interface has more than one gateway route for some
    // reason, only the first one found is considered.
    WinLuid bestGatewayIpv4, bestGatewayIpv6;
    unsigned long bestMetricIpv4{0}, bestMetricIpv6{0};
    for(unsigned long i=0; i<pRouteTable.get()->NumEntries; ++i)
    {
        const auto &route = pRouteTable.get()->Table[i];
        WinLuid routeItfLuid{route.InterfaceLuid};

        // We are only interested in default gateway routes.
        if(route.DestinationPrefix.PrefixLength != 0)
            continue;

        // Get the interface row corresponding to this route's address family
        InterfaceInfo *pItf = getItfForLuid(routeItfLuid);
        const MIB_IPINTERFACE_ROW *pItfRow{};
        if(pItf)
        {
            if(route.DestinationPrefix.Prefix.si_family == AF_INET)
                pItfRow = pItf->pItf4;
            else if(route.DestinationPrefix.Prefix.si_family == AF_INET6)
                pItfRow = pItf->pItf6;
        }

        // Tolerate a missing interface - route and interface changes aren't
        // synchronized, we might have missing interfaces during a transient.
        if(!pItf || !pItfRow)
        {
            qWarning() << "Ignoring gateway route via"
                << parseWinSockaddr(route.NextHop).toString() << "on interface"
                << routeItfLuid << "=" << route.InterfaceIndex
                << "- can't find interface";
            continue;
        }

        // Consequence of the above; any family other than AF_INET or AF_INET6
        // was ignored due to not finding an interface row
        Q_ASSERT(route.DestinationPrefix.Prefix.si_family == AF_INET ||
                 route.DestinationPrefix.Prefix.si_family == AF_INET6);

        // Ignore interfaces that are not connected.  The routes still usually
        // exist after an interface is disconnected, netstat -nr seems to hide
        // them.
        if(!pItfRow->Connected)
        {
            qInfo() << "Ignoring gateway route via"
                << parseWinSockaddr(route.NextHop).toString() << "on interface"
                << routeItfLuid << "=" << route.InterfaceIndex
                << "- interface is not connected";
            continue;
        }

        // If we already saw a gateway route for this interface and this
        // protocol, ignore this one.  Otherwise, store the gateway.
        WinLuid *pBestGateway{};
        unsigned long *pBestMetric;
        if(route.DestinationPrefix.Prefix.si_family == AF_INET)
        {
            if(pItf->gatewayIpv4 != kapps::core::Ipv4Address{})
            {
                qInfo() << "Ignoring gateway route via"
                    << parseWinSockaddr(route.NextHop).toString() << "on interface"
                    << routeItfLuid << "=" << route.InterfaceIndex
                    << "- already saw gateway" << pItf->gatewayIpv4
                    << "for this interface";
                continue;
            }
            pItf->gatewayIpv4 = kapps::core::Ipv4Address{ntohl(route.NextHop.Ipv4.sin_addr.S_un.S_addr)};
            pBestGateway = &bestGatewayIpv4;
            pBestMetric = &bestMetricIpv4;
        }
        else
        {
            if(pItf->gatewayIpv6 != kapps::core::Ipv6Address{})
            {
                qInfo() << "Ignoring gateway route via"
                    << parseWinSockaddr(route.NextHop).toString() << "on interface"
                    << routeItfLuid << "=" << route.InterfaceIndex
                    << "- already saw gateway" << pItf->gatewayIpv6
                    << "for this interface";
                continue;
            }
            pItf->gatewayIpv6 = kapps::core::Ipv6Address{route.NextHop.Ipv6.sin6_addr.u.Byte};
            pBestGateway = &bestGatewayIpv6;
            pBestMetric = &bestMetricIpv6;
        }

        // Windows combines the route metric with the interface metric to
        // compare routes (this is also what netstat -nr prints)
        unsigned long combinedMetric = route.Metric + pItfRow->Metric;
        if(!*pBestGateway || combinedMetric < *pBestMetric)
        {
            *pBestGateway = routeItfLuid;
            *pBestMetric = combinedMetric;
        }
    }

    // If an IPv4 route was found, find the local addresses and build a
    // NetworkConnection
    static const WinNativeWifi::InterfaceMap emptyInterfaceMap{};
    // If we can't connect to Native Wifi, we'll assume all interfaces are
    // wired.
    const auto &wifiInterfaces = _pWifi ? _pWifi->interfaces() : emptyInterfaceMap;
    std::vector<NetworkConnection> connections;
    connections.reserve(interfacesByLuid.size());
    for(auto &itf : interfacesByLuid)
    {
        connections.push_back({});
        connections.back().networkInterface(QString::number(itf.first.value()));
        connections.back().defaultIpv4(itf.first == bestGatewayIpv4);
        connections.back().defaultIpv6(itf.first == bestGatewayIpv6);
        connections.back().gatewayIpv4(itf.second.gatewayIpv4);
        connections.back().gatewayIpv6(itf.second.gatewayIpv6);
        connections.back().addressesIpv4(std::move(itf.second.addressesIpv4));
        connections.back().addressesIpv6(std::move(itf.second.addressesIpv6));
        connections.back().mtu4(itf.second.pItf4 ? itf.second.pItf4->NlMtu : 0);
        connections.back().mtu6(itf.second.pItf6 ? itf.second.pItf6->NlMtu : 0);

        // Check if this is a Wi-Fi interface and apply that state
        auto itWifiState = wifiInterfaces.find(itf.first);
        if(itWifiState == wifiInterfaces.end())
        {
            // Assume any non-Wifi interface is a wired interface.  Not 100%
            // correct for things like cellular data connections, but reasonable
            // for most interfaces.
            connections.back().medium(NetworkConnection::Medium::Wired);
        }
        else
        {
            connections.back().medium(NetworkConnection::Medium::WiFi);
            if(itWifiState->second.associated)
            {
                connections.back().wifiAssociated(true);
                connections.back().wifiEncrypted(itWifiState->second.encrypted);
                connections.back().parseWifiSsid(
                    reinterpret_cast<const char *>(itWifiState->second.ssid),
                    itWifiState->second.ssidLength);
            }
        }
    }

    return connections;
}

void WinNetworks::updateConnections()
{
    try
    {
        updateNetworks(readRoutes());
    }
    catch(const Error &ex)
    {
        qWarning() << "Unable to read routing table to find network connections -"
            << ex;
        // We don't know what the network connections are at this point
        updateNetworks({});
    }
}

void WinNetworks::connectNativeWifi()
{
    Q_ASSERT(!_pWifi);  // Ensured by caller
    try
    {
        qInfo() << "Connect to Native Wifi now, service is up";
        _pWifi.emplace();
    }
    catch(const Error &ex)
    {
        // If we can't connect, we won't be able to get Wi-Fi adapter
        // information.  This might happen if a connection attempt races
        // with the service shutting down, in which case we will get a
        // notification when it comes back up.
        qWarning() << "Failed to connect to Native Wifi:" << ex;
    }
}

void WinNetworks::onWlanSvcStateChange(WinServiceState::State newState)
{
    qInfo() << "WLAN AutoConfig service is now in state" << traceEnum(newState);
    switch(newState)
    {
        // No change occurs in these states - the service status is more or less
        // "unknown".
        // In Pause/Continue states, the service might resume (meaning any
        // existing connection is fine), but we shouldn't try to connect now.
        default:
        case WinServiceState::State::Initializing:
        case WinServiceState::State::ContinuePending:
        case WinServiceState::State::PausePending:
        case WinServiceState::State::Paused:
            break;
        // The service is up - try to connect if we're not connected
        case WinServiceState::State::Running:
        {
            if(!_pWifi)
            {
                connectNativeWifi();
                if(_pWifi)
                {
                    // It succeeded, so update the current state with Wi-Fi
                    // information.
                    updateConnections();
                }
            }
            break;
        }
        // The service is down - disconnect if we were connected, the connection
        // is no longer valid (we must reconnect if it comes back up, we will
        // not get updates on this connection).
        case WinServiceState::State::StartPending:
        case WinServiceState::State::StopPending:
        case WinServiceState::State::Stopped:
        case WinServiceState::State::Deleted:
        {
            if(_pWifi)
            {
                // This is not really good - we won't be able to report Wi-Fi
                // state.  Connections will probably go down anyway, but this
                // does not normally happen.
                qWarning() << "Disconnected from Native Wifi, service is down."
                    << "Wifi state will not be available.";
                _pWifi.clear();
                updateConnections();
            }
            break;
        }
    }
}

std::unique_ptr<NetworkMonitor> createWinNetworks()
{
    return std::make_unique<WinNetworks>();
}
