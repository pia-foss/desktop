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

#include <common/src/common.h>
#include "wg_service_json.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <WS2tcpip.h> // inet_pton()

namespace WgServiceJson
{

bool hexToWgKey(const QString &hexStr, wg_key &key)
{
    if(hexStr.length() != 64) // 32 bytes = 64 hex chars
    {
        qWarning() << "Invalid hex key length:" << hexStr.length() << "(expected 64)";
        return false;
    }

    for(int i = 0; i < 32; ++i)
    {
        bool ok;
        key[i] = static_cast<uint8_t>(hexStr.mid(i * 2, 2).toUInt(&ok, 16));
        if(!ok)
        {
            qWarning() << "Failed to parse hex key at position" << i;
            return false;
        }
    }
    return true;
}

bool parseEndpoint(const QString &endpointStr, decltype(wg_peer::endpoint) &endpoint)
{
    auto parts = endpointStr.split(':');
    if(parts.size() != 2)
    {
        qWarning() << "Invalid endpoint format:" << endpointStr << "(expected IP:port)";
        return false;
    }

    QString ipStr = parts[0];
    QString portStr = parts[1];

    bool portOk;
    uint16_t port = portStr.toUShort(&portOk);
    if(!portOk)
    {
        qWarning() << "Invalid port in endpoint:" << portStr;
        return false;
    }

    // Try IPv4 first
    in_addr addr4{};
    if(::inet_pton(AF_INET, ipStr.toUtf8().constData(), &addr4) == 1)
    {
        endpoint.addr4.sin_family = AF_INET;
        endpoint.addr4.sin_addr = addr4;
        endpoint.addr4.sin_port = htons(port);
        return true;
    }

    // Try IPv6
    in6_addr addr6{};
    if(::inet_pton(AF_INET6, ipStr.toUtf8().constData(), &addr6) == 1)
    {
        endpoint.addr6.sin6_family = AF_INET6;
        endpoint.addr6.sin6_addr = addr6;
        endpoint.addr6.sin6_port = htons(port);
        return true;
    }

    qWarning() << "Failed to parse IP address:" << ipStr;
    return false;
}

bool parseCIDR(const QString &cidrStr, wg_allowedip &allowedIp)
{
    auto parts = cidrStr.split('/');
    if(parts.size() != 2)
    {
        qWarning() << "Invalid CIDR format:" << cidrStr << "(expected IP/mask)";
        return false;
    }

    QString ipStr = parts[0];
    QString maskStr = parts[1];

    bool maskOk;
    uint8_t cidr = static_cast<uint8_t>(maskStr.toUInt(&maskOk));
    if(!maskOk)
    {
        qWarning() << "Invalid CIDR mask:" << maskStr;
        return false;
    }

    // Try IPv4 first
    in_addr addr4{};
    if(::inet_pton(AF_INET, ipStr.toUtf8().constData(), &addr4) == 1)
    {
        if(cidr > 32)
        {
            qWarning() << "Invalid IPv4 CIDR mask:" << cidr << "(max 32)";
            return false;
        }
        allowedIp.family = AF_INET;
        allowedIp.ip4 = addr4;
        allowedIp.cidr = cidr;
        allowedIp.next_allowedip = nullptr;
        return true;
    }

    // Try IPv6
    in6_addr addr6{};
    if(::inet_pton(AF_INET6, ipStr.toUtf8().constData(), &addr6) == 1)
    {
        if(cidr > 128)
        {
            qWarning() << "Invalid IPv6 CIDR mask:" << cidr << "(max 128)";
            return false;
        }
        allowedIp.family = AF_INET6;
        allowedIp.ip6 = addr6;
        allowedIp.cidr = cidr;
        allowedIp.next_allowedip = nullptr;
        return true;
    }

    qWarning() << "Failed to parse IP address in CIDR:" << ipStr;
    return false;
}

std::shared_ptr<WgDevStatus> parseWgServiceJson(const QByteArray &jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if(parseError.error != QJsonParseError::NoError)
    {
        qWarning() << "JSON parse error:" << parseError.errorString() << "at offset"
                   << parseError.offset;
        return nullptr;
    }

    if(!doc.isObject())
    {
        qWarning() << "JSON root is not an object";
        return nullptr;
    }

    QJsonObject root = doc.object();

    // Create the WgDevStatus object
    auto pStatus = std::make_shared<WgDevStatus>();
    wg_device &dev = pStatus->device();

    // Initialize device structure
    std::memset(&dev, 0, sizeof(dev));

    // Parse interface name
    if(root.contains("name"))
    {
        QString name = root["name"].toString();
        QByteArray nameUtf8 = name.toUtf8(); // Store to extend lifetime
        std::strncpy(dev.name, nameUtf8.constData(), sizeof(dev.name) - 1);
        dev.name[sizeof(dev.name) - 1] = '\0';
    }
    else
    {
        qWarning() << "Missing 'name' field in JSON";
        return nullptr;
    }

    // Parse public key (hex-encoded)
    if(root.contains("publicKey"))
    {
        QString pubKeyHex = root["publicKey"].toString();
        if(hexToWgKey(pubKeyHex, dev.public_key))
        {
            dev.flags = static_cast<wg_device_flags>(dev.flags | WGDEVICE_HAS_PUBLIC_KEY);
        }
        else
        {
            qWarning() << "Failed to parse public key";
            return nullptr;
        }
    }

    // Parse listen port
    if(root.contains("listenPort"))
    {
        dev.listen_port = static_cast<uint16_t>(root["listenPort"].toInt());
        dev.flags = static_cast<wg_device_flags>(dev.flags | WGDEVICE_HAS_LISTEN_PORT);
    }

    // Parse fwmark (Linux only, but present in JSON)
    if(root.contains("fwmark"))
    {
        dev.fwmark = static_cast<uint32_t>(root["fwmark"].toInt());
        dev.flags = static_cast<wg_device_flags>(dev.flags | WGDEVICE_HAS_FWMARK);
    }

    // Note: privateKey is not included in JSON output (for security)
    // So we don't set WGDEVICE_HAS_PRIVATE_KEY flag

    // Parse peers array
    if(!root.contains("peers"))
    {
        qWarning() << "Missing 'peers' field in JSON";
        return nullptr;
    }

    QJsonArray peersArray = root["peers"].toArray();
    for(const QJsonValue &peerValue : peersArray)
    {
        if(!peerValue.isObject())
        {
            qWarning() << "Peer entry is not an object";
            continue;
        }

        QJsonObject peerObj = peerValue.toObject();

        // Create peer structure
        wg_peer peer{};
        std::memset(&peer, 0, sizeof(peer));

        // Parse peer public key
        if(peerObj.contains("publicKey"))
        {
            QString peerKeyHex = peerObj["publicKey"].toString();
            if(hexToWgKey(peerKeyHex, peer.public_key))
            {
                peer.flags = static_cast<wg_peer_flags>(peer.flags | WGPEER_HAS_PUBLIC_KEY);
            }
            else
            {
                qWarning() << "Failed to parse peer public key";
                continue;
            }
        }
        else
        {
            qWarning() << "Peer missing 'publicKey' field";
            continue;
        }

        // Parse endpoint (no flag needed - it's always part of the structure)
        if(peerObj.contains("endpoint"))
        {
            QString endpointStr = peerObj["endpoint"].toString();
            if(!parseEndpoint(endpointStr, peer.endpoint))
            {
                qWarning() << "Failed to parse endpoint";
                // Continue anyway - endpoint is optional
            }
        }

        // Parse traffic statistics
        if(peerObj.contains("txBytes"))
        {
            peer.tx_bytes = peerObj["txBytes"].toVariant().toULongLong();
        }
        if(peerObj.contains("rxBytes"))
        {
            peer.rx_bytes = peerObj["rxBytes"].toVariant().toULongLong();
        }

        // Parse persistent keepalive
        if(peerObj.contains("persistentKeepalive"))
        {
            peer.persistent_keepalive_interval =
                static_cast<uint16_t>(peerObj["persistentKeepalive"].toInt());
            peer.flags =
                static_cast<wg_peer_flags>(peer.flags | WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL);
        }

        // Parse lastHandshake (optional - only present after handshake occurs)
        // Format: ISO 8601 datetime string (e.g., "2025-12-01T09:49:59.6998107+01:00")
        if(peerObj.contains("lastHandshake"))
        {
            QString timestampStr = peerObj["lastHandshake"].toString();
            // Parse ISO 8601 datetime string to Unix timestamp
            QDateTime dateTime = QDateTime::fromString(timestampStr, Qt::ISODate);
            if(dateTime.isValid())
            {
                peer.last_handshake_time.tv_sec = dateTime.toSecsSinceEpoch();
                peer.last_handshake_time.tv_nsec =
                    dateTime.time().msec() * 1000000; // Convert ms to ns
            }
            else
            {
                qWarning() << "Failed to parse lastHandshake timestamp:" << timestampStr;
                peer.last_handshake_time.tv_sec = 0;
                peer.last_handshake_time.tv_nsec = 0;
            }
        }
        else
        {
            // No handshake yet
            peer.last_handshake_time.tv_sec = 0;
            peer.last_handshake_time.tv_nsec = 0;
        }

        // Note: presharedKey is not included in JSON output
        // Leave it as zeros (no PSK)

        // Add peer to status (this copies the peer and manages memory)
        pStatus->addPeer(peer);

        // Parse allowed IPs
        if(peerObj.contains("allowedIps"))
        {
            QJsonArray allowedIpsArray = peerObj["allowedIps"].toArray();
            for(const QJsonValue &ipValue : allowedIpsArray)
            {
                QString cidrStr = ipValue.toString();
                wg_allowedip allowedIp{};
                if(parseCIDR(cidrStr, allowedIp))
                {
                    pStatus->addAllowedIp(allowedIp);
                }
                else
                {
                    qWarning() << "Failed to parse allowed IP:" << cidrStr;
                }
            }
        }
    }

    qInfo() << "Successfully parsed WireGuard configuration:" << dev.name << "with"
            << peersArray.size() << "peer(s)";

    return pStatus;
}

} // namespace WgServiceJson
