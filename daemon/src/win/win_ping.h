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
#line HEADER_FILE("win_ping.h")

#ifndef WIN_PING_H
#define WIN_PING_H

#include "win/win_util.h"
#include <QPointer>
#include <QWinEventNotifier>
#include <vector>
#include <Windows.h>

// Send an ICMP echo and receive the response with WinIcmpEcho::send().  This
// uses IcmpSendEcho2().
class WinIcmpEcho : public QObject
{
    Q_OBJECT

public:
    // Send an ICMP echo and receive the response using a WinIcmpEcho.
    // The WinIcmpEcho is self-owning - it will destroy itself when the response
    // is received or the request times out.  (No signal is emitted if a timeout
    // occurs.)
    //
    // Since the WinIcmpEcho is self-owning, you can only connect to its signals
    // or store the reference in a QPointer - don't store references to it any
    // other way.
    //
    // There is no way to cancel an ICMP echo request on Windows, so a timeout
    // is required (this is also why the object is self-owning).
    //
    // If the request can't be sent, returns nullptr.
    static QPointer<WinIcmpEcho> send(quint32 address,
                                      std::chrono::milliseconds timeout,
                                      WORD payloadSize = 32,
                                      bool allowFragments = true);

private:
    WinIcmpEcho(WinHandle event, std::size_t replyBufSize);

private:
    bool shouldTraceIcmpError(DWORD status) const;
    QString ipErrorString(DWORD status) const;
    void onEventActivated();

signals:
    // Emitted when the reply is received.
    void receivedReply(quint32 address);
    void receivedError(int errCode);

private:
    // Event signaled when the reply is received
    WinHandle _event;
    // Buffer provided to IcmpSendEcho2() for the reply
    std::vector<quint8> _replyBuffer;
    QWinEventNotifier _notifier;
};

#endif
