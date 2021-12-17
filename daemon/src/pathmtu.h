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

#ifndef PATHMTU_H
#define PATHMTU_H

#include "common.h"
#include "exec.h"
#include "thread.h"
#include "settings.h"
#include "vpn.h"
#include <QObject>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>
#include <chrono>

#if defined(Q_OS_WIN)
#include "win/win_ping.h"
#else
#include "posix/posix_ping.h"
#endif

class MtuPinger : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("pathmtu");

private:
    static Executor _executor;

public:
    // Create MtuPinger with:
    // - The tunnel adapter (where the MTU will be applied, and through which
    //   MTU probes are sent).  On Windows, this is a WinNetworkAdapter.
    // - The maximum possible tunnel MTU, based on the physical interface MTU
    //   and the known encapsulation overhead for the protocol in use
    // - The value of the MTU setting for this connection (-1 => auto,
    //   0 => large packets, >0 => specific MTU ("small packets" is 1250))
    //
    // If auto is selected, MtuPinger starts probing to detect the actual MTU
    // and applies the results to the tunnel adapter.
    //
    // Otherwise, MtuPinger applies a specific MTU determined by the maximum
    // MTU and the MTU setting, then does not probe anything.
    MtuPinger(std::shared_ptr<NetworkAdapter> pTunnelAdapter, int maxTunnelMtu,
              int mtuSetting);

private:
    void start();
    void receivedReply(quint32 addr);
    void receivedError(int errCode);
    void timeout();
    void applyMtu(int mtu);

private:
    std::shared_ptr<NetworkAdapter> _pTunnelAdapter;
    QTimer _pingTimeout;
#if defined(Q_OS_UNIX)
    PosixPing _ping;
#endif
    int _mtu, _goodMtu, _badMtu;
    int _retryCounter;
};

#endif // PATHMTU_H
