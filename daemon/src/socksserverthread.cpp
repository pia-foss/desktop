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
#line SOURCE_FILE("socksserverthread.cpp")

#include "socksserverthread.h"

SocksServerThread::SocksServerThread()
    : _port{0}
{
}

void SocksServerThread::start(QHostAddress bindAddress, QString bindInterface)
{
    // Checked by caller
    Q_ASSERT(bindAddress.protocol() == QAbstractSocket::NetworkLayerProtocol::IPv4Protocol);

    _thread.invokeOnThread([&]()
    {
        if(_pSocksServer)
        {
            qInfo() << "Updating SOCKS server bind address to" << bindAddress;
            // Set the new bind address.  It does not seem possible to actually
            // reach this since the proxy is stopped when we leave the Connected
            // state, but this is here just in case.
            _pSocksServer->updateBindAddress(bindAddress, bindInterface);
        }
        else
        {
            _pSocksServer = new SocksServer{bindAddress, bindInterface};
            _pSocksServer->setParent(&_thread.objectOwner());
            _port = _pSocksServer->port();
            _password = _pSocksServer->password();
            if(!_port)
            {
                qWarning() << "Unable to start SOCKS server";
                delete _pSocksServer.data();
            }
            else
            {
                qInfo() << "Started SOCKS server on port" << _port
                    << "with bind address" << bindAddress;
            }
        }
    });
}

void SocksServerThread::stop()
{
    _thread.invokeOnThread([&]()
    {
        if(_pSocksServer)
        {
            qInfo() << "Stopping SOCKS server";
            delete _pSocksServer.data();
            _port = 0;
        }
        else
        {
            qInfo() << "SOCKS server was not running, nothing to stop";
        }
    });
}
