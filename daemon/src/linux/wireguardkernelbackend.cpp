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
#line SOURCE_FILE("wireguardkernelbackend.cpp")

#include "wireguardkernelbackend.h"
#include <cstring>
#include "brand.h"
#include "exec.h"

void WireguardKernelBackend::cleanup()
{
    // - This is called before attempting to connect too in case there were
    //   remnants from an earlier daemon, cleanup must be idempotent and
    //   shouldn't log warnings if no connection was set up.

    int err = ::wg_del_device(interfaceName.data());
    // Ignore ENODEV, device does not exist
    if(err && err != -(ENODEV))
    {
        qWarning() << "Can't delete device" << interfaceName << "- error"
            << err;
    }
}

WireguardKernelBackend::WireguardKernelBackend()
    : _created{false}
{
}

WireguardKernelBackend::~WireguardKernelBackend()
{
    if(_created)
    {
        // Tear down - same as initial cleanup for kernel backend
        cleanup();
    }
}

auto WireguardKernelBackend::createInterface(wg_device &wgDev,
                                             const QPair<QHostAddress, int> &)
    -> Async<std::shared_ptr<NetworkAdapter>>
{
    // Specify the device.
    // Length of interfaceName checked with static_assert above.
    Q_ASSERT(interfaceName.size() <= static_cast<int>(sizeof(wgDev.name)-1));
    // Count is sizeof(wgDev.name)-1 to ensure there's a null terminator
    // (strncpy doesn't normally do this); wgDev was zeroed at initialization
    std::strncpy(wgDev.name, interfaceName.data(), sizeof(wgDev.name)-1);

    int err = ::wg_add_device(interfaceName.data());
    if(err)
    {
        qWarning() << "Can't create interface" << interfaceName << "- error"
            << err;
        throw Error{HERE, Error::Code::WireguardCreateDeviceFailed};
    }

    // Interface was created, we need to do cleanup on destruction, even if
    // something below fails.
    _created = true;

    err = ::wg_set_device(&wgDev);
    if(err)
    {
        qWarning() << "Can't configure device" << interfaceName << "- error"
            << err;
        throw Error{HERE, Error::Code::WireguardConfigDeviceFailed};
    }

    return Async<std::shared_ptr<NetworkAdapter>>::resolve(std::make_shared<NetworkAdapter>(interfaceName));
}

auto WireguardKernelBackend::getStatus() -> Async<WgDevPtr>
{
    wg_device *pDevRaw{nullptr};
    int err = ::wg_get_device(&pDevRaw, interfaceName.data());
    // Put it in an owning pointer
    WgDevPtr pDev{pDevRaw, &::wg_free_device};
    if(err || !pDev)
    {
        qWarning() << "Can't find wireguard device" << interfaceName
            << "for stats -" << err;
        // Reject synchronously
        return Async<WgDevPtr>::reject(Error{HERE, Error::Code::WireguardDeviceLost});
    }

    // Resolve synchronously
    return Async<WgDevPtr>::resolve(pDev);
}

Async<void> WireguardKernelBackend::shutdown()
{
    // There's no asynchronous shutdown to do for the kernel backend; the
    // destructor does synchronous cleanup.
    return Async<void>::resolve();
}
