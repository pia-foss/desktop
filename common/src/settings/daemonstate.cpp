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
#include "daemonstate.h"

DaemonState::DaemonState()
    : NativeJsonObject(DiscardUnknownProperties)
{
    // If any property of a ServiceLocations object changes, consider the
    // ServiceLocations property changed also.  (Daemon does not monitor for
    // nested property changes.)
    auto connectServiceLocations = [this](ServiceLocations &locs, auto func)
    {
        connect(&locs, &ServiceLocations::chosenLocationChanged, this, func);
        connect(&locs, &ServiceLocations::bestLocationChanged, this, func);
        connect(&locs, &ServiceLocations::nextLocationChanged, this, func);
    };

    connectServiceLocations(_vpnLocations, [this](){
        emitPropertyChange({[this](){emit vpnLocationsChanged();},
                            QStringLiteral("vpnLocations")});
    });
    connectServiceLocations(_shadowsocksLocations, [this](){
        emitPropertyChange({[this](){emit shadowsocksLocationsChanged();},
                            QStringLiteral("shadowsocksLocations")});
    });

    auto connectConnectionInfo = [this](ConnectionInfo &info, auto func)
    {
        connect(&info, &ConnectionInfo::vpnLocationChanged, this, func);
        connect(&info, &ConnectionInfo::vpnLocationAutoChanged, this, func);
        connect(&info, &ConnectionInfo::proxyChanged, this, func);
        connect(&info, &ConnectionInfo::proxyCustomChanged, this, func);
        connect(&info, &ConnectionInfo::proxyShadowsocksChanged, this, func);
        connect(&info, &ConnectionInfo::proxyShadowsocksLocationAutoChanged, this, func);
    };

    connectConnectionInfo(_connectingConfig, [this](){
        emitPropertyChange({[this](){emit connectingConfigChanged();},
                            QStringLiteral("connectingConfig")});
    });
    connectConnectionInfo(_connectedConfig, [this](){
        emitPropertyChange({[this](){emit connectedConfigChanged();},
                            QStringLiteral("connectedConfig")});
    });
}
