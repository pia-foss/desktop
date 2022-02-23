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
#line HEADER_FILE("socksserverthread.h")

#ifndef SOCKSSERVERTHREAD_H
#define SOCKSSERVERTHREAD_H

#include "socksserver.h"
#include "thread.h"
#include <QPointer>

// SocksServerThread just runs a SocksServer on a worker thread.  The server can
// be started and stopped.
class SocksServerThread : public QObject
{
    Q_OBJECT

public:
    SocksServerThread();

public:
    // Start the SOCKS server, or update the bind address if it is already
    // running.  port() will be nonzero following this call if the server
    // starts; otherwise it will be 0.
    void start(QHostAddress bindAddress, QString bindInterface);

    // Stop the SOCKS server if it is running.
    void stop();

    quint16 port() const {return _port;}
    const QByteArray &password() const {return _password;}

private:
    RunningWorkerThread _thread;
    QPointer<SocksServer> _pSocksServer;
    quint16 _port;
    QByteArray _password;
};

#endif
