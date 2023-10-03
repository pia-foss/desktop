// Copyright (c) 2023 Private Internet Access, Inc.
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
#line HEADER_FILE("wireguardbackend.h")

#ifndef WIREGUARD_BACKEND_H
#define WIREGUARD_BACKEND_H

#include <QHostAddress>
#include <common/src/async.h>
#include "vpn.h"
#include <deque>

// wireguard.h will include Windows headers on Windows; include winapi.h first
// to exclude things we don't want like the min()/max() macros
#include <kapps_core/src/winapi.h>
// embeddable-wg-library - C header
extern "C"
{
    #include <wireguard.h>
}

// This is a complete wg_device with buffers for the peers and allowed IPs.
// There are two general use cases for this:
// - Copying a wg_device from an unknown source (just use the wg_device ctor)
// - Building a wg_device with peers and allowed IPs from scratch
//   - You can set up a wg_device locally and use the wg_device ctor, or use
//     WgDevStatus::device() to set up the device
//   - Use addPeer() / addAllowedIp() to add peers / allowed IPs
class WgDevStatus
{
public:
    // Copy operations preserve self-referential pointers
    WgDevStatus() = default;
    WgDevStatus(const wg_device &dev);
    WgDevStatus(const WgDevStatus &other);
    WgDevStatus(WgDevStatus &&) = default;
    ~WgDevStatus() = default;

public:
    WgDevStatus &operator=(const WgDevStatus &other);
    WgDevStatus &operator=(WgDevStatus &&) = default;

public:
    // Get the complete wg_device.  Mutable access is permitted to set up the
    // wg_device, but do _not_ modify first_peer/last_peer, or the peers'
    // first_allowedip/last_allowedip.
    wg_device &device() {return _dev;}
    const wg_device &device() const {return _dev;}
    // Add a peer to the end of the wg_device
    wg_peer &addPeer(const wg_peer &peer);
    // Add an allowed IP to the end of the last peer
    // There must be at least one peer
    wg_allowedip &addAllowedIp(const wg_allowedip &allowedip);

private:
    wg_device _dev;
    // _peers and _allowedIps are just storage for the wg_peer and wg_allowedip
    // objects; they are linked into _dev.
    std::deque<wg_peer> _peers;
    std::deque<wg_allowedip> _allowedIps;
};

// WireguardBackend defines the interface to the various backend implementations
// of Wireguard:
// - The Windows service method (userspace)
// - wireguard-go (Mac/Linux, userspace)
// - Linux kernel module (kernel)
//
// Most of the Wireguard connection method is the same - authentication, the
// device configuration, timeouts, stat updates, etc., so all Wireguard backends
// use one VPNMethod (WireguardMethod).
//
// The part that varies per platform is creating/configuring the actual device,
// routing, DNS, etc.  WireguardBackend implementations provide this.
class WireguardBackend : public QObject
{
    Q_OBJECT

public:
    // The preferred Wireguard interface name, used by implementations that can
    // choose the interface name.  This may not be used by all implementations,
    // and it may not be the name of the actual network interface (for userspace
    // methods on a tun/utun device).
    static const QLatin1String interfaceName;

public:
    // Owning pointer for a wg_device.  Implementations of getStatus() typically
    // return an aliased wg_device, which will destroy an object owning the
    // wg_device/wg_peer/wg_allowedip objects (this depends on how they were
    // allocated).
    using WgDevPtr = std::shared_ptr<wg_device>;

public:
    // shutdown() will be called before destroying the backend to permit
    // asynchronous shutdown (even if createInterface() was not called or
    // failed), but the backend will still be destroyed if the shutdown task
    // times out or rejects.
    virtual ~WireguardBackend() = default;

protected:
    // Inform WireguardMethod that an error occurred
    void raiseError(const Error &err);

public:
    // Create and configure the Wireguard interface with the given Wireguard
    // device configuration, and peer IP/mask.
    //
    // The WireguardBackend may modify the wg_device before returning to set
    // platform-specific parameters and/or a device name, etc.
    //
    // A NetworkAdapter is provided as the result of the returned Task, which
    // includes the device name.  Once the task resolves, a subsequent call to
    // getStatus() must succeed unless the device has been lost (there cannot be
    // a delay until getStatus() would succeed for the first time).
    //
    // If this fails, the WireguardBackend implementation throws an Error or
    // rejects the task.
    //
    // The peer IP is provided by WireguardMethod (it's not part of wg_device),
    // but it is are only used by the backend on Windows.  On Mac/Linux,
    // WireguardMethod itself configures the interface, because the
    // configuration method depends on the OS but not the selected backend (both
    // Linux backends use the same configuration).  On Windows, the Wireguard
    // service applies the configuration, so we have to pass it to the backend
    // so it is provided in the config file.
    //
    // A WireguardBackend should only be used to create one interface
    // (implementations can throw if this is called after an interface was
    // already created).
    //
    // To tear down the interface, call shutdown().
    virtual auto createInterface(wg_device &wgDev,
                                 const QPair<QHostAddress, int> &peerIpNet)
        -> Async<std::shared_ptr<NetworkAdapter>> = 0;

    // Get the current status of the device.  May reject if the device cannot be
    // found, has not been created yet, etc.  If the task resolves, the pointer
    // must be valid.
    virtual Async<WgDevPtr> getStatus() = 0;

    // Shut down the device; called before the WireguardBackend is destroyed.
    // If shutdown times out, or the task is rejected, the backend will still be
    // destroyed.
    virtual Async<void> shutdown() = 0;
signals:
    void error(const Error &err);
};

// Encode a WireGuard key in base64.
QString wgKeyToB64(const wg_key &key);

#endif
