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
#line SOURCE_FILE("wireguardbackend.cpp")

#include "wireguardbackend.h"
#include "brand.h"

namespace
{
    // Compile-time length of string literal (as long as the argument is a
    // constant)
    size_t constexpr literalLength(const char *value)
    {
        return (value && *value) ? (1 + literalLength(value+1)) : 0;
    }

    // The interface name is "wgpia0", "wgacme0", etc.  It can be anything, but
    // this follows the recommended Wireguard convention.
    constexpr const char *rawInterfaceName{"wg" BRAND_CODE "0"};
    static_assert(literalLength(rawInterfaceName) < sizeof(wg_device::name)-1,
                  "Brand code is too long - Wireguard interface name exceeds IFNAMESIZ");
}

WgDevStatus::WgDevStatus(const wg_device &dev)
    : _dev{dev}
{
    // Deep-copy the peers
    _dev.first_peer = _dev.last_peer = nullptr;
    for(const wg_peer *pPeer = dev.first_peer; pPeer; pPeer = pPeer->next_peer)
    {
        addPeer(*pPeer);
    }
}

WgDevStatus::WgDevStatus(const WgDevStatus &other)
    : WgDevStatus{other.device()}
{
}

WgDevStatus &WgDevStatus::operator=(const WgDevStatus &other)
{
    *this = {other.device()};
    return *this;
}

wg_peer &WgDevStatus::addPeer(const wg_peer &peer)
{
    _peers.push_back(peer);
    if(_dev.last_peer)
        _dev.last_peer->next_peer = &_peers.back();
    else
        _dev.first_peer = &_peers.back();
    _dev.last_peer = &_peers.back();
    _peers.back().first_allowedip = _peers.back().last_allowedip = nullptr;

    // Deep-copy the allowed IPs
    for(const wg_allowedip *pAllowedIp = peer.first_allowedip;
        pAllowedIp;
        pAllowedIp = pAllowedIp->next_allowedip)
    {
        addAllowedIp(*pAllowedIp);
    }

    return *_dev.last_peer;
}

wg_allowedip &WgDevStatus::addAllowedIp(const wg_allowedip &allowedIp)
{
    Q_ASSERT(_dev.last_peer);   // Must have a peer; ensured by caller

    _allowedIps.push_back(allowedIp);
    if(_dev.last_peer->last_allowedip)
        _dev.last_peer->last_allowedip->next_allowedip = &_allowedIps.back();
    else
        _dev.last_peer->first_allowedip = &_allowedIps.back();
    _dev.last_peer->last_allowedip = &_allowedIps.back();

    return *_dev.last_peer->last_allowedip;
}

const QLatin1String WireguardBackend::interfaceName{rawInterfaceName};

void WireguardBackend::raiseError(const Error &err)
{
    qWarning() << "WireGuard backend error:" << err;
    emit error(err);
}

QString wgKeyToB64(const wg_key &key)
{
    auto base64Ascii = QByteArray::fromRawData(reinterpret_cast<const char*>(&key[0]), sizeof(key)).toBase64();
    return QString::fromLatin1(base64Ascii);
}
