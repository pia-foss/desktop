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
#line SOURCE_FILE("preconnectstatus.cpp")

#include "preconnectstatus.h"
#include "client.h"
#include <QCoreApplication>

#if defined(Q_OS_MAC)
#include "mac/mac_install.h"
#endif

PreConnectStatus::PreConnectStatus(DaemonConnection &daemonConnection)
    : _daemonConnection{daemonConnection}
{
    // Client creates this before connecting
    Q_ASSERT(!_daemonConnection.isConnected());
    connect(&_daemonConnection, &DaemonConnection::connectedChanged, this,
            &PreConnectStatus::daemonConnectedChanged);
}

void PreConnectStatus::daemonConnectedChanged(bool connected)
{
    if(connected)
    {
        _daemonConnection.disconnect(this);
        emit initialConnect();
    }
}

void PreConnectStatus::reinstall()
{
#if defined(Q_OS_MAC)
    macExecuteInstaller();
    qInfo() << "Quit due to starting installer for reinstall";
    QCoreApplication::quit();
#else
    qCritical() << "Cannot reinstall from client on this platform";
#endif
}

bool PreConnectStatus::getCanReinstall() const
{
#if defined(Q_OS_MAC)
    return true;
#else
    return false;
#endif
}
