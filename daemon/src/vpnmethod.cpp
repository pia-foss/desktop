// Copyright (c) 2024 Private Internet Access, Inc.
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
#line SOURCE_FILE("vpnmethod.cpp")

#include "vpnmethod.h"

VPNMethod::VPNMethod(QObject *pParent, const OriginalNetworkScan &netScan)
    : QObject{pParent}, _state{State::Created}, _netScan{netScan}
{
}

void VPNMethod::updateNetwork(const OriginalNetworkScan &newNetwork)
{
    if(newNetwork != _netScan)
    {
        qInfo() << "Updating netScan for VPN method, old:" << _netScan << "new:" << newNetwork;
        _netScan = newNetwork;
        networkChanged();
    }
}

void VPNMethod::advanceState(State newState)
{
    if(newState < _state)
    {
        // The state cannot go backwards.
        qError() << "Attempted to revert from state" << traceEnum(_state)
            << "to earlier state" << traceEnum(newState);
        return;
    }

    // Don't need to do anything if the state is the same
    if(newState > _state)
    {
        qInfo() << "State advanced from" << traceEnum(_state) << "to"
            << traceEnum(newState);
        _state = newState;
        emit stateChanged();
    }
}

void VPNMethod::emitTunnelConfiguration(const QString &deviceName,
                                        const QString &deviceLocalAddress,
                                        const QString &deviceRemoteAddress)
{
    emit tunnelConfiguration(deviceName, deviceLocalAddress,
                             deviceRemoteAddress);
}

void VPNMethod::emitBytecounts(quint64 received, quint64 sent)
{
    emit bytecount(received, sent);
}

void VPNMethod::emitFirewallParamsChanged()
{
    emit firewallParamsChanged();
}

void VPNMethod::raiseError(const Error &err)
{
    qInfo() << "VPN method error:" << err;
    emit error(err);
}
