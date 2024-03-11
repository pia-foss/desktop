// Copyright (c) 2024 Private Internet Access, Inc.
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

#ifndef POSIX_PING_H
#define POSIX_PING_H

#include <common/src/common.h>
#include <kapps_core/src/posix/posix_objects.h>
#include <QSocketNotifier>

// Open an ICMP socket and send pings on Mac/Linux.
// An identifier is chosen randomly when the object is created; responses are
// detected based on that identifier.
//
// It is possible (but unlikely) that duplicate or spurious responses could be
// emitted if they happen to have that identifier.
class PosixPing : public QObject
{
    Q_OBJECT

private:
    enum
    {
        // Size of payload included in echoes
        PayloadSize = 56
    };
    struct IcmpEcho
    {
        quint8 type;
        quint8 code;
        quint16 checksum;
        quint16 identifier;
        quint16 sequence;
    };

public:
    PosixPing();

private:
    quint16 calcChecksum(const quint8 *data, std::size_t len) const;

public:
    // Send an ICMP echo request.  If a reply is received, it will be signaled
    // with receivedReply().
    bool sendEchoRequest(quint32 address, int payloadSize = 32, bool allowFragment = true);

private:
    void onReadyRead();

signals:
    void receivedReply(quint32 address);

private:
    kapps::core::PosixFd _icmpSocket;
    // Have to delay construction of this notifier until we have set up the
    // ICMP socket.
    nullable_t<QSocketNotifier> _pReadNotifier;
    quint16 _identifier;
    quint16 _nextSequence;
};

#endif
