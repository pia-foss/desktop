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

#include "common.h"
#line HEADER_FILE("win_interfacemonitor.h")

#ifndef WIN_INTERFACEMONITOR_H
#define WIN_INTERFACEMONITOR_H

#include "daemon.h" // NetworkAdapter
#include "win.h"

class WinNetworkAdapter : public NetworkAdapter
{
public:
    WinNetworkAdapter(const IP_ADAPTER_ADDRESSES &adapter)
        : NetworkAdapter(adapter.AdapterName),
          _luid{adapter.Luid.Value}, _indexIpv4{adapter.IfIndex},
          _description{QString::fromWCharArray(adapter.Description)},
          _friendlyName{QString::fromWCharArray(adapter.FriendlyName)},
          _savedMetricValueIPv4{-1},
          _savedMetricValueIPv6{-1}
    {
    }

    virtual void setMetricToLowest() override;
    virtual void restoreOriginalMetric() override;
    quint64 luid() const {return _luid;}
    DWORD indexIpv4() const {return _indexIpv4;}
    const QString &description() const {return _description;}
    const QString &friendlyName() const {return _friendlyName;}

private:
    // Any value less than 5 should work fine. 3 is chosen without any real reason.
    enum { interfaceMetric = 3 };
    quint64 _luid;
    DWORD _indexIpv4;
    // Some names are captured just for diagnostics
    QString _description;
    QString _friendlyName;
    int _savedMetricValueIPv4, _savedMetricValueIPv6;
    void setMetric(const ADDRESS_FAMILY family, ULONG metric);
    int getMetric(const ADDRESS_FAMILY);
};

inline QDebug &operator<<(QDebug &d, const WinNetworkAdapter &adapter)
{
    return d << adapter.devNode() << "-" << adapter.description() << "-"
        << adapter.friendlyName() << "-" << adapter.luid();
}

// WinInterfaceMonitor monitors for network interface additions/deletions on
// Windows.  It uses NotifyIpInterfaceChange() to detect when any interface is
// added or removed, and emits changed() when that occurs.
//
// Because the signals are emitted asynchronously, slots connected to this
// signal might not notice any change between two consecutive signals, but if
// they re-check the state of the relevant interface for each signal, they'll
// always end up observing the correct final state at least once.
class WinInterfaceMonitor : public QObject
{
    Q_OBJECT

private:
    static void WINAPI ipChangeCallback(PVOID callerContext,
                                        PMIB_IPINTERFACE_ROW pRow,
                                        MIB_NOTIFICATION_TYPE notificationType);

public:
    // Not using AutoSingleton because it doesn't tear down the object - it's
    // just leaked.  NotifyIpInterfaceChange() starts an internal worker thread,
    // we need to tear it down.
    static WinInterfaceMonitor &instance();

private:
    template<class AdapterPredicateFunc>
    static auto getAllNetworkAdapters(AdapterPredicateFunc predicate)
        -> QList<std::shared_ptr<WinNetworkAdapter>>;

public:
    // Get all network adapters with a given description.
    // Not really related to the interface change implementation, but these are
    // usually used together.
    // May throw if the adapters can't be retrieved.
    static auto getDescNetworkAdapters(const wchar_t *pDesc)
        -> QList<std::shared_ptr<WinNetworkAdapter>>;

    // Get the network adapter for a given LUID.
    static auto getAdapterForLuid(quint64 luid)
        -> std::shared_ptr<WinNetworkAdapter>;

    // Get all network adapters, excluding loopback
    static auto getNetworkAdapters()
        -> QList<std::shared_ptr<WinNetworkAdapter>>;

private:
    WinInterfaceMonitor();
    ~WinInterfaceMonitor();

signals:
    // Any interface change has occurred; including add/delete/configuration
    // changed.
    void configChanged();
    // An interface has been added or deleted (config changes do not cause this
    // signal.)
    void interfacesChanged();

private:
    HANDLE _ipNotificationHandle;
};

#endif
