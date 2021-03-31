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

#include "common.h"
#ifndef PIA_FINDER_H
#define PIA_FINDER_H

#include <libproc.h>  // for proc_pidpath()
#include <QSet>
#include "vpn.h"      // For OriginalNetworkScan
#include "mac/mac_splittunnel_types.h"

class AddressAndPort
{
public:
    AddressAndPort(quint32 ip, quint16 port)
    : _ip{ip}
    , _port{port}
    {}

    AddressAndPort(const AddressAndPort &other)
    : AddressAndPort(other._ip, other._port)
    {}

    bool operator==(const AddressAndPort &other) const { return _ip == other._ip && _port == other._port; }
    bool operator!=(const AddressAndPort &other) const { return !(*this == other); }

public:
    quint32 ip() const {return _ip;}
    quint16 port() const {return _port;}
private:
    quint32 _ip;
    quint16 _port;
};

inline uint qHash(const AddressAndPort &address)
{
    return qHash(address.ip()) ^ qHash(address.port());
}

namespace PortFinder
{
    // The maximum number of PIDs we support
enum { maxPids = 16384 };

QSet<pid_t> pids(const QVector<QString> &paths);
PortSet ports(const QSet<pid_t> &pids, IPVersion ipVersion, const OriginalNetworkScan &netScan);
PortSet ports(const QVector<QString> &paths, IPVersion ipVersion, const OriginalNetworkScan &netScan);
QSet<AddressAndPort> addresses4(const QVector<QString> &paths);
pid_t pidForPort(quint16 port, IPVersion ipVersion=IPv4);

bool matchesPath(const QVector<QString> &paths, pid_t pid);
QString pidToPath(pid_t);

template <typename Func_T>
pid_t pidFor(Func_T func)
{
    int totalPidCount = 0;
    QVector<pid_t> allPidVector;
    allPidVector.resize(maxPids);

    // proc_listallpids() returns the total number of PIDs in the system
    // (assuming that maxPids is > than the total PIDs, otherwise it returns maxPids)
    totalPidCount = proc_listallpids(allPidVector.data(), maxPids * sizeof(pid_t));

    for (int i = 0; i != totalPidCount; ++i)
    {
        pid_t pid = allPidVector[i];

        // Add the PID to our set if matches one of the paths
        if(func(pid))
            return pid;
    }

    return 0;
}

template <typename Func_T>
QSet<pid_t> pidsFor(Func_T func)
{
    int totalPidCount = 0;
    QSet<pid_t> pidsForPaths;
    QVector<pid_t> allPidVector;
    allPidVector.resize(maxPids);

    // proc_listallpids() returns the total number of PIDs in the system
    // (assuming that maxPids is > than the total PIDs, otherwise it returns maxPids)
    totalPidCount = proc_listallpids(allPidVector.data(), maxPids * sizeof(pid_t));

    if(totalPidCount == maxPids)
    {
        qWarning() << "Reached max PID count" << maxPids
            << "- some processes may not be identified";
    }

    for(int i = 0; i != totalPidCount; ++i)
    {
        pid_t pid = allPidVector[i];

        // Add the PID to our set if matches one of the paths
        if(func(pid))
            pidsForPaths.insert(pid);
    }

    return pidsForPaths;
}
}
#endif
