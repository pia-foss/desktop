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
#line SOURCE_FILE("cliclient.cpp")

#include "cliclient.h"

CliClient::CliClient()
    : _connection{nullptr}
{
    _connection.connectToDaemon();

    // Trace the state just for diagnostics
    QObject::connect(&_connection, &DaemonConnection::connectedChanged, this,
                     [](bool connected)
    {
        qInfo() << "CLI connected:" << connected;
    });
    QObject::connect(&_connection, &DaemonConnection::connectedChanged, this,
                     &CliClient::checkFirstConnected);
}

void CliClient::checkFirstConnected(bool connected)
{
    if(connected)
    {
        QObject::disconnect(&_connection, &DaemonConnection::connectedChanged,
                            this, &CliClient::checkFirstConnected);
        emit firstConnected();
    }
}
