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
#line HEADER_FILE("wireguardkernelbackend.h")

#ifndef WIREGUARDKERNELBACKEND_H
#define WIREGUARDKERNELBACKEND_H

#include "wireguardbackend.h"
#include "vpn.h"

// WireguardKernelBackend is a backend Wireguard implementation using the Linux
// kernel module.  It uses embeddable-wg-library to configure the interface
// using Netlink.
class WireguardKernelBackend : public WireguardBackend
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("wireguardkernelbackend")

public:
    // Do cleanup at startup in case anything was left over from a crashed
    // daemon
    static void cleanup();

public:
    WireguardKernelBackend();
    virtual ~WireguardKernelBackend() override;

public:
    virtual auto createInterface(wg_device &wgDev,
                                 const QPair<QHostAddress, int> &peerIpNet)
        -> Async<std::shared_ptr<NetworkAdapter>> override;
    virtual Async<WgDevPtr> getStatus() override;
    virtual Async<void> shutdown() override;

private:
    // Whether we have created an interface - just indicates whether we should
    // do cleanup at destruction
    bool _created;
};

#endif
