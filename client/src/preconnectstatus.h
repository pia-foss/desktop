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
#line HEADER_FILE("preconnectstatus.h")

#ifndef PRECONNECTSTATUS_H
#define PRECONNECTSTATUS_H

#include "daemonconnection.h"
#include <QObject>

// PreConnectStatus exposes the initial connection status information and
// methods to the splash screen window.
class PreConnectStatus : public QObject
{
    Q_OBJECT

public:
    explicit PreConnectStatus(DaemonConnection &daemonConnection);

private:
    void daemonConnectedChanged(bool connected);

signals:
    // Emitted when the initial connection completes and we're about to show the
    // main window.
    void initialConnect();

public:
    // Whether it is possible to reinstall from the client itself on this
    // platform
    Q_PROPERTY(bool canReinstall READ getCanReinstall FINAL CONSTANT)

    // Execute a reinstall - only possible when canReinstall is true.
    Q_INVOKABLE void reinstall();

    bool getCanReinstall() const;

private:
    DaemonConnection &_daemonConnection;
};

#endif
