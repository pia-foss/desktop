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

#include <common/src/common.h>
#line HEADER_FILE("cliclient.h")

#ifndef CLICLIENT_H
#define CLICLIENT_H

#include <clientlib/src/daemonconnection.h>

// CliClient creates a DaemonConnection and supporting objects to run the CLI
// interface.
class CliClient : public QObject
{
    Q_OBJECT

public:
    CliClient();

public:
    DaemonConnection &connection() {return _connection;}

private:
    void checkFirstConnected(bool connected);

signals:
    // Emitted _only_ for the first established daemon connection - the first
    // time DaemonConnection::connectedChanged() is called with connected=true.
    // Most one-shot commands use this to ensure they only perform the requested
    // action once.
    void firstConnected();

private:
    DaemonConnection _connection;
    // TODO - UnixSignalHandler
    // TODO - MessageReceiver for Windows exit on uninstall?
};

#endif
