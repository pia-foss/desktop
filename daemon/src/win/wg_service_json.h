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

#pragma once
#include "../wireguardbackend.h"
#include <QByteArray>
#include <memory>

// Utilities for parsing JSON output from wgservice.exe /getconfig
// This replaces the old UAPI (text-based) protocol that used named pipes
namespace WgServiceJson
{
// Parse JSON from wgservice.exe /getconfig output into WgDevStatus
//
// JSON format from wgservice.exe:
// {
//   "name": "wgpia0",
//   "type": "kernel",
//   "publicKey": "hex...",          // Hex-encoded (not base64!)
//   "listenPort": 51971,
//   "fwmark": 0,
//   "peers": [
//     {
//       "publicKey": "hex...",       // Hex-encoded (not base64!)
//       "endpoint": "IP:port",
//       "allowedIps": ["CIDR", ...],
//       "txBytes": 12345,
//       "rxBytes": 67890,
//       "persistentKeepalive": 25,
//       "lastHandshake": 1234567890  // Optional - omitted when zero
//     }
//   ]
// }
//
// Returns nullptr on parsing errors (malformed JSON, missing required fields)
std::shared_ptr<WgDevStatus> parseWgServiceJson(const QByteArray &jsonData);

// Convert hex-encoded string to wg_key (32 bytes)
// Format: 64 hex characters (e.g., "ed141cee184ce745...")
// Returns false if the string is not valid hex or wrong length
bool hexToWgKey(const QString &hexStr, wg_key &key);

// Parse endpoint string "IP:port" to sockaddr structure
// Format: "158.173.36.34:12719"
// Supports both IPv4 and IPv6
// Returns false if parsing fails
bool parseEndpoint(const QString &endpointStr, decltype(wg_peer::endpoint) &endpoint);

// Parse CIDR notation "IP/mask" to wg_allowedip structure
// Format: "0.0.0.0/1" or "128.0.0.0/1"
// Supports both IPv4 and IPv6
// Returns false if parsing fails
bool parseCIDR(const QString &cidrStr, wg_allowedip &allowedIp);
}
