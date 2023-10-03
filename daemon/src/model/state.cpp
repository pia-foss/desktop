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
#include "state.h"
#include <nlohmann/json.hpp>

ConnectionInfo::ConnectionInfo(QSharedPointer<const Location> pVpnLocation,
    bool vpnLocationAuto, QString method, bool methodForcedByAuth,
    QString dnsType, QString openvpnCipher, bool otherAppsUseVpn,
    QString proxy, QString proxyCustom,
    QSharedPointer<const Location> pProxyShadowsocks,
    bool proxyShadowsocksLocationAuto, bool portForward)
    : _pVpnLocation{std::move(pVpnLocation)}, _vpnLocationAuto{vpnLocationAuto},
      _method{std::move(method)}, _methodForcedByAuth{methodForcedByAuth},
      _dnsType{std::move(dnsType)}, _openvpnCipher{std::move(openvpnCipher)},
      _otherAppsUseVpn{otherAppsUseVpn}, _proxy{std::move(proxy)},
      _proxyCustom{std::move(proxyCustom)},
      _pProxyShadowsocks{std::move(pProxyShadowsocks)},
      _proxyShadowsocksLocationAuto{proxyShadowsocksLocationAuto},
      _portForward{portForward}
{
}
