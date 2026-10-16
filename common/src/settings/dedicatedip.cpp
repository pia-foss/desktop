// Copyright (c) 2026 Private Internet Access, Inc.
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

#include "../common.h"
#include "dedicatedip.h"
#include <QDateTime>

namespace
{
    // Read one OpenVPN port list from the DIP "ports" object.  A missing or
    // null entry is treated as empty (the DIP doesn't offer that protocol);
    // any other malformed value throws.
    std::vector<quint16> readDipPorts(const QJsonObject &ports, const char *entry,
                                      const QString &dipId)
    {
        const QJsonValue value = ports.value(QLatin1String{entry});
        if(value.isUndefined() || value.isNull())
        {
            qWarning() << "Dedicated IP" << dipId << "has no" << entry << "ports";
            return {};
        }
        return json_cast<std::vector<quint16>>(value, HERE);
    }
}

void applyDedicatedIpJson(const QJsonObject &tokenData,
                          AccountDedicatedIp &dipInfo)
{
    // Read everything first so dipInfo is unchanged if the response is
    // malformed.
    //
    // API returns the expire time in seconds; we store timestamps in
    // milliseconds
    quint64 newExpire = json_cast<quint64>(tokenData["dip_expire"], HERE) * 1000;
    QString regionId = json_cast<QString>(tokenData["id"], HERE);
    std::vector<QString> serviceGroups = json_cast<std::vector<QString>>(tokenData["groups"], HERE);
    QString ip = json_cast<QString>(tokenData["ip"], HERE);
    QString cn = json_cast<QString>(tokenData["cn"], HERE);
    // DIP over OpenVPN uses DIP-specific ports.  A missing "ports" field means
    // the DIP has no OpenVPN ports; WireGuard can still be used.
    std::vector<quint16> openvpnUdpPorts, openvpnTcpPorts;
    const QJsonValue portsValue = tokenData["ports"];
    if(portsValue.isUndefined() || portsValue.isNull())
    {
        qWarning() << "Dedicated IP" << dipInfo.id() << "has no OpenVPN ports";
    }
    else
    {
        const QJsonObject ports = json_cast<QJsonObject>(portsValue, HERE);
        openvpnUdpPorts = readDipPorts(ports, "ovpnudp", dipInfo.id());
        openvpnTcpPorts = readDipPorts(ports, "ovpntcp", dipInfo.id());
    }

    if(newExpire == 0)
    {
        qWarning() << "Dedicated IP" << dipInfo.id()
            << "returned invalid expiration 0";
        // This shouldn't happen, but just in case, do not ever store an
        // expiration timestamp of 0 - this would make the region look like a
        // non-DIP region.
        //
        // We should store _something_ though since this might be a new DIP,
        // just use 1000 instead (1 second after midnight, Jan 1, 1970 -
        // functionally equivalent assuming no time travel)
        newExpire = 1000;
    }
    if(newExpire != dipInfo.expire())
    {
        qInfo() << "Dedicated IP" << dipInfo.id() << "updated expiration from"
            << dipInfo.expire() << "to" << newExpire;
        dipInfo.expire(newExpire);
    }
    QString oldIp = dipInfo.ip();
    dipInfo.regionId(regionId);
    dipInfo.serviceGroups(serviceGroups);
    dipInfo.ip(ip);
    dipInfo.cn(cn);
    dipInfo.openvpnUdpPorts(openvpnUdpPorts);
    dipInfo.openvpnTcpPorts(openvpnTcpPorts);
    // If the dedicated IP was known and has changed, set the last change
    // timestamp.  Don't clear this if it didn't change, the timestamps are only
    // cleared when the user dismisses the notification.
    if(!oldIp.isEmpty() && oldIp != dipInfo.ip())
    {
        qInfo() << "Dedicated IP" << dipInfo.id() << "has changed IP address";
        dipInfo.lastIpChange(QDateTime::currentMSecsSinceEpoch());
    }
}
