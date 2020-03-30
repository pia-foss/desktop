// Copyright (c) 2020 Private Internet Access, Inc.
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
#line HEADER_FILE("wireguardservicebackend.h")

#ifndef WIREGUARDSERVICEBACKEND_H
#define WIREGUARDSERVICEBACKEND_H

#include "wireguardbackend.h"
#include "wireguarduapi.h"
#include "async.h"
#include <QLocalSocket>

// WireguardServiceBackend is a Wireguard userspace implementation using the
// Windows service backend.
//
// Configuration occurs by writing a config file that's specified on the
// service's command line.  Stat updates use the Wireguard userspace IPC
// protocol.
class WireguardServiceBackend : public WireguardBackend
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("wireguardservicebackend")

private:
    static const QString &pipePath();

    static Async<void> asyncCleanup();

public:
    // Do cleanup of this method in case anything was left over from a crashed
    // daemon (stops the PIA WG service if it is running).
    static void cleanup();

public:
    WireguardServiceBackend();
    virtual ~WireguardServiceBackend() override;

public:
    virtual auto createInterface(wg_device &wgDev,
                                 const QPair<QHostAddress, int> &peerIpNet)
        -> Async<std::shared_ptr<NetworkAdapter>> override;

    virtual Async<WgDevPtr> getStatus() override;

    virtual Async<void> shutdown() override;
};

#endif
