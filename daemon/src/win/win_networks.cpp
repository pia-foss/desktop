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
#line SOURCE_FILE("win_networks.cpp")

#include "win_networks.h"
#include "win.h"
#include "win/win_util.h"
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

private:
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
    : _routeNotificationHandle{}, _unicastIpNotificationHandle{},
      _ipInterfaceNotificationHandle{}
{
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

    WinMibPtr<MIB_IPFORWARD_TABLE2> pRouteTable;
    tableResult = ::GetIpForwardTable2(AF_UNSPEC, pRouteTable.receive());
    if(tableResult != NO_ERROR || !pRouteTable)
    {
        qWarning() << "Unable to read routing table -" << tableResult;
        throw Error{HERE, Error::Code::Unknown};
    }

    // Find an interface row for an address family and LUID - returns nullptr if
    // it's not found
    auto findInterface = [&](ADDRESS_FAMILY family, ULONG64 luidValue) -> const MIB_IPINTERFACE_ROW*
    {
        for(unsigned long i=0; i<pItfTable.get()->NumEntries; ++i)
        {
            const MIB_IPINTERFACE_ROW &row = pItfTable.get()->Table[i];
            if(row.Family == family && row.InterfaceLuid.Value == luidValue)
                return &row;
        }
        qWarning() << "Interface for LUID" << luidValue << "and family"
            << family << "was not found";
        return nullptr;
    };

    unsigned long bestIpv4Metric{0}, bestIpv6Metric{0};
    const MIB_IPFORWARD_ROW2 *pBestIpv4Gateway{nullptr}, *pBestIpv6Gateway{nullptr};

    for(unsigned long i=0; i<pRouteTable.get()->NumEntries; ++i)
    {
        const auto &route = pRouteTable.get()->Table[i];

        // We are only interested in default gateway routes.
        if(route.DestinationPrefix.PrefixLength != 0)
            continue;

        // Find the interface for this route - we need to know if it's connected
        // and what its route metric is
        auto pItf = findInterface(route.DestinationPrefix.Prefix.si_family,
                                  route.InterfaceLuid.Value);
        // Tolerate a missing interface - route and interface changes aren't
        // synchronized, we might have missing interfaces during a transient.
        if(!pItf)
        {
            qWarning() << "Ignoring gateway route via"
                << parseWinSockaddr(route.NextHop).toString() << "on interface"
                << route.InterfaceLuid.Value << "=" << route.InterfaceIndex
                << "- can't find interface";
            continue;
        }

        // Ignore interfaces that are not connected.  The routes still usually
        // exist after an interface is disconnected, netstat -nr seems to hide
        // them.
        if(!pItf->Connected)
        {
            qInfo() << "Ignoring gateway route via"
                << parseWinSockaddr(route.NextHop).toString() << "on interface"
                << route.InterfaceLuid.Value << "=" << route.InterfaceIndex
                << "- interface is not connected";
            continue;
        }

        // Windows combines the route metric with the interface metric to
        // compare routes (this is also what netstat -nr prints)
        unsigned long combinedMetric = route.Metric + pItf->Metric;
        switch(route.DestinationPrefix.Prefix.si_family)
        {
            case AF_INET:
                if(!pBestIpv4Gateway || combinedMetric < bestIpv4Metric)
                {
                    bestIpv4Metric = combinedMetric;
                    pBestIpv4Gateway = &route;
                }
                break;
            case AF_INET6:
                if(!pBestIpv6Gateway || combinedMetric < bestIpv6Metric)
                {
                    bestIpv6Metric = combinedMetric;
                    pBestIpv6Gateway = &route;
                }
                break;
            default:
                break;
        }
    }

    // If an IPv4 route was found, find the local addresses and build a
    // NetworkConnection
    std::vector<NetworkConnection> connections;
    connections.reserve(2);
    if(pBestIpv4Gateway)
    {
        std::vector<std::pair<Ipv4Address, unsigned>> addresses;
        for(unsigned long i=0; i<pAddrTable.get()->NumEntries; ++i)
        {
            const auto &addr = pAddrTable.get()->Table[i];
            if(addr.InterfaceLuid.Value == pBestIpv4Gateway->InterfaceLuid.Value &&
               addr.Address.si_family == AF_INET)
            {
                addresses.push_back({Ipv4Address{ntohl(addr.Address.Ipv4.sin_addr.S_un.S_addr)},
                                     addr.OnLinkPrefixLength});
            }
        }
        Ipv4Address gateway4{ntohl(pBestIpv4Gateway->NextHop.Ipv4.sin_addr.S_un.S_addr)};
        connections.push_back({QString::number(pBestIpv4Gateway->InterfaceLuid.Value),
                               true, false, gateway4,
                               {}, std::move(addresses), {}});
    }

    // If an IPv6 route was found, find the local addresses and build a
    // NetworkConnection
    if(pBestIpv6Gateway)
    {
        std::vector<std::pair<Ipv6Address, unsigned>> addresses;
        for(unsigned long i=0; i<pAddrTable.get()->NumEntries; ++i)
        {
            const auto &addr = pAddrTable.get()->Table[i];
            if(addr.InterfaceLuid.Value == pBestIpv6Gateway->InterfaceLuid.Value &&
               addr.Address.si_family == AF_INET6)
            {
                addresses.push_back({Ipv6Address{addr.Address.Ipv6.sin6_addr.u.Byte},
                                     addr.OnLinkPrefixLength});
                // Ignore link-local
                if(addresses.back().first.isLinkLocal())
                    addresses.pop_back();
            }
        }

        Ipv6Address gateway6{pBestIpv6Gateway->NextHop.Ipv6.sin6_addr.u.Byte};

        // If it's a different interface from the IPv4 gateway, create a new
        // connection.  If they're the same, add this information to the
        // existing connection.
        if(!pBestIpv4Gateway || pBestIpv6Gateway->InterfaceLuid.Value != pBestIpv4Gateway->InterfaceLuid.Value)
        {
            connections.push_back({});
            connections.back().networkInterface(QString::number(pBestIpv6Gateway->InterfaceLuid.Value));
        }

        connections.back().defaultIpv6(true);
        connections.back().gatewayIpv6(gateway6);
        connections.back().addressesIpv6(std::move(addresses));
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

std::unique_ptr<NetworkMonitor> createWinNetworks()
{
    return std::make_unique<WinNetworks>();
}
