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
#line SOURCE_FILE("win_interfacemonitor.cpp")

#include "win_interfacemonitor.h"
#include <cstddef>

void WinNetworkAdapter::setMetricToLowest()
{
    if (!luid()) return;

    auto currentMetricIPv4 = getMetric(AF_INET);
    auto currentMetricIPv6 = getMetric(AF_INET6);
    qInfo() << "Update metrics from" << currentMetricIPv4 << "/"
        << currentMetricIPv4 << "to" << interfaceMetric;

    if (currentMetricIPv4 != interfaceMetric && currentMetricIPv4 != -1) {
        _savedMetricValueIPv4 = currentMetricIPv4;
        setMetric(AF_INET, interfaceMetric);
        qInfo() << "Set interface IPv4 metric from" << _savedMetricValueIPv4
            << "to" << interfaceMetric;
    }

    if (currentMetricIPv6 != interfaceMetric && currentMetricIPv6 != -1) {
        _savedMetricValueIPv6 = currentMetricIPv6;
        setMetric(AF_INET6, interfaceMetric);
        qInfo() << "Set interface IPv6 metric from" << _savedMetricValueIPv6
            << "to" << interfaceMetric;
    }
}

void WinNetworkAdapter::restoreOriginalMetric()
{
    if (!luid()) return;

    qInfo() << "Restore saved metrics:" << _savedMetricValueIPv4 << "/"
        << _savedMetricValueIPv6;

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
    ipiface.InterfaceLuid.Value = luid();
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
    ipiface.InterfaceLuid.Value = luid();
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


void WINAPI WinInterfaceMonitor::ipChangeCallback(PVOID callerContext,
                                                  PMIB_IPINTERFACE_ROW pRow,
                                                  MIB_NOTIFICATION_TYPE notificationType)
{
    // Signal any add/delete notification.  In principle, we could try to figure
    // out whether the notification actually affects any relevant adapter, but
    // re-checking for any change is straightforward and adapter adds/deletes
    // are not common.
    //
    // Signal the initial notification too - it's unlikely that anything is
    // connected to the signal at this point, but it does not hurt anything.
    //
    // Note that pRow only has a few fields filled in; the doc explains how you
    // would use it to actually get a full MIB_IPINTERFACE_ROW:
    // https://docs.microsoft.com/en-us/windows/desktop/api/netioapi/nf-netioapi-notifyipinterfacechange
    Q_UNUSED(pRow)
    if(notificationType == MibAddInstance ||
        notificationType == MibDeleteInstance ||
        notificationType == MibInitialNotification)
    {
        // This callback occurs on a worker thread specifically for this
        // purpose.  Queue a call over to WinInterfaceMonitor on the service
        // thread.
        WinInterfaceMonitor *pThis = reinterpret_cast<WinInterfaceMonitor*>(callerContext);
        Q_ASSERT(pThis);    // Ensured by ctor
        QMetaObject::invokeMethod(pThis, &WinInterfaceMonitor::changed,
                                  Qt::QueuedConnection);
    }
}

template<class AdapterPredicateFunc>
auto WinInterfaceMonitor::getAllNetworkAdapters(AdapterPredicateFunc predicate)
    -> QList<std::shared_ptr<WinNetworkAdapter>>
{
    QList<std::shared_ptr<WinNetworkAdapter>> adapters;

    int iterations = 0;

    ULONG error;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_WINS_INFO | GAA_FLAG_INCLUDE_GATEWAYS;
    ULONG size = 15000;

    std::unique_ptr<std::byte[]> pAddrBuf;

    do
    {
        pAddrBuf.reset(new std::byte[size]);

        error = GetAdaptersAddresses(AF_UNSPEC, flags, NULL,
                                     reinterpret_cast<IP_ADAPTER_ADDRESSES*>(pAddrBuf.get()),
                                     &size);
        if(error != ERROR_BUFFER_OVERFLOW)
            break;
    } while (++iterations < 3);

    if (error != ERROR_SUCCESS)
        throw SystemError(HERE, error);

    IP_ADAPTER_ADDRESSES *addresses{reinterpret_cast<IP_ADAPTER_ADDRESSES*>(pAddrBuf.get())};
    for (auto address = addresses; address; address = address->Next)
    {
        if (address && predicate(*address))
        {
            auto adapter = std::make_shared<WinNetworkAdapter>(*address);
            adapters.append(std::move(adapter));
        }
    }

    return adapters;
}

auto WinInterfaceMonitor::getDescNetworkAdapters(const wchar_t *pDesc)
    -> QList<std::shared_ptr<WinNetworkAdapter>>
{
    Q_ASSERT(pDesc);    // Ensured by caller
    return getAllNetworkAdapters([pDesc](const IP_ADAPTER_ADDRESSES &addresses)
    {
        return addresses.Description && wcsstr(addresses.Description, pDesc);
    });
}

auto WinInterfaceMonitor::getAdapterForLuid(quint64 luid)
    -> std::shared_ptr<WinNetworkAdapter>
{
    auto adapters = getAllNetworkAdapters([luid](const IP_ADAPTER_ADDRESSES &addresses)
    {
        return addresses.Luid.Value == luid;
    });
    // LUID should be unique - there shouldn't be more than one such adapter.
    // Trace if it happens somehow.
    if(adapters.size() > 1)
    {
        qWarning() << "Found" << adapters.size() << "adapters for LUID" << luid;
    }
    if(adapters.empty())
    {
        qWarning() << "Could not find adapter for LUID" << luid;
        return {};
    }
    return adapters.front();
}

auto WinInterfaceMonitor::getNetworkAdapters()
    -> QList<std::shared_ptr<WinNetworkAdapter>>
{
    return getAllNetworkAdapters([](const IP_ADAPTER_ADDRESSES &addresses) { return addresses.IfType != IF_TYPE_SOFTWARE_LOOPBACK; });
}

WinInterfaceMonitor &WinInterfaceMonitor::instance()
{
    static WinInterfaceMonitor _inst;
    return _inst;
}

WinInterfaceMonitor::WinInterfaceMonitor()
    : _ipNotificationHandle{}
{
    auto notifyResult = ::NotifyIpInterfaceChange(AF_UNSPEC, &ipChangeCallback,
                                                  reinterpret_cast<void*>(this),
                                                  TRUE, &_ipNotificationHandle);
    if(notifyResult != NO_ERROR)
    {
        qWarning() << "Couldn't enable interface change notifications:" << notifyResult;
    }
}

WinInterfaceMonitor::~WinInterfaceMonitor()
{
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
}
