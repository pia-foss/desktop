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

#pragma once
#include "../clientlib.h"
#include <common/src/json.h>
#include <common/src/settings/connection.h>
#include <common/src/settings/automation.h>

// This is the client's model of the daemon state.
//
// DaemonState includes information describing the daemon's current state, which
// is not persisted - it is reconstructed from the persisted settings and data
// models.
//
// *************************
// * Static property model *
// *************************
//
// This is intended to eventually be eliminated in favor of a dynamic JSON-style
// representation of the daemon state, but for now this is still handled with
// NativeJsonObject.
//
// Most of the GUI client - the QML parts - already use a dynamic model.  The
// C++ parts of the GUI client rarely use DaemonState.  The CLI client does use
// DaemonState from C++, but it could generally just throw for type mismatches
// or other unexpected conditions, this would produce the correct CLI error.
//
// The key issue still requiring the statically-listed properties are fields
// with a common invariant, such as connectionState and connectedServer, which
// must be present when connectionState is "Connected".  NativeJsonObject
// batches update notifications so that an intermediate state is never observed.
//
// It doesn't seem possible to preserve that behavior without defining
// properties statically:
// * QML doesn't support dynamic QObject properties
// * QQmlPropertyMap cannot defer update notifications during a batch
//
// The proper solution is to group interrelated properties into a single
// object property, like "connection.state" and "connection.server".  Then the
// "connection" property can be updated atomically.
class CLIENTLIB_EXPORT DaemonState : public NativeJsonObject
{
    Q_OBJECT

public:
    DaemonState();

    JsonField(bool, hasAccountToken, {})
    JsonField(bool, vpnEnabled, {})
    JsonField(QString, connectionState, {})
    JsonField(bool, usingSlowInterval, {})
    JsonField(bool, needsReconnect, {})
    JsonField(uint64_t, bytesReceived, {})
    JsonField(uint64_t, bytesSent, {})
    JsonField(int, forwardedPort, {})
    JsonField(QString, externalIp, {})
    JsonField(QString, externalVpnIp, {})
    JsonField(QJsonValue, chosenTransport, {})
    JsonField(QJsonValue, actualTransport, {})
    JsonField(QJsonObject, vpnLocations, {})
    JsonField(QJsonObject, shadowsocksLocations, {})
    JsonField(QJsonObject, connectingConfig, {})
    JsonField(QJsonObject, connectedConfig, {})
    JsonField(QJsonObject, nextConfig, {})
    JsonField(QJsonValue, connectedServer, {})
    JsonField(QJsonObject, availableLocations, {})
    JsonField(QJsonObject, regionsMetadata, {})
    JsonField(QJsonArray, groupedLocations, {})
    JsonField(QJsonArray, dedicatedIpLocations, {})
    JsonField(QJsonArray, openvpnUdpPortChoices, {})
    JsonField(QJsonArray, openvpnTcpPortChoices, {})
    JsonField(QJsonArray, intervalMeasurements, {})
    JsonField(qint64, connectionTimestamp, {})
    JsonField(QStringList, overridesFailed, {})
    JsonField(QStringList, overridesActive, {})
    JsonField(qint64, openVpnAuthFailed, {})
    JsonField(qint64, connectionLost, {})
    JsonField(qint64, proxyUnreachable, {})
    JsonField(bool, killswitchEnabled, {})
    JsonField(QString, availableVersion, {})
    JsonField(bool, osUnsupported, {})
    JsonField(int, updateDownloadProgress, {})
    JsonField(QString, updateInstallerPath, {})
    JsonField(qint64, updateDownloadFailure, {})
    JsonField(QString, updateVersion, {})
    JsonField(bool, tapAdapterMissing, {})
    JsonField(bool, wintunMissing, {})
    JsonField(QString, netExtensionState, {})
    JsonField(bool, connectionProblem, {})
    JsonField(quint64, dedicatedIpExpiring, {})
    JsonField(int, dedicatedIpDaysRemaining, {})
    JsonField(quint64, dedicatedIpChanged, {})
    JsonField(qint64, dnsConfigFailed, {})
    JsonField(bool, invalidClientExit, {})
    JsonField(bool, killedClient, {})
    JsonField(qint64, hnsdFailing, {})
    JsonField(qint64, hnsdSyncFailure, {})
    JsonField(QString, originalGatewayIp, {})
    JsonField(QString, originalInterfaceIp, {})
    JsonField(unsigned, originalInterfaceNetPrefix, {})
    JsonField(QString, originalInterface, {})
    JsonField(QString, originalInterfaceIp6, {})
    JsonField(QString, originalGatewayIp6, {})
    JsonField(QString, macosPrimaryServiceKey, {})
    JsonField(qint64, snoozeEndTime, -1)
    JsonField(std::vector<QString>, splitTunnelSupportErrors, {})
    JsonField(std::vector<QString>, vpnSupportErrors, {})
    JsonField(QString, tunnelDeviceName, {})
    JsonField(QString, tunnelDeviceLocalAddress, {})
    JsonField(QString, tunnelDeviceRemoteAddress, {})
    JsonField(bool, wireguardAvailable, {})
    JsonField(bool, wireguardKernelSupport, {})
    JsonField(std::vector<quint32>, existingDNSServers, {})
    JsonField(std::vector<QString>, automationSupportErrors, {})
    JsonField(QJsonValue, automationLastTrigger, {})
    JsonField(QJsonValue, automationCurrentMatch, {})
    JsonField(QJsonArray, automationCurrentNetworks, {})
};
