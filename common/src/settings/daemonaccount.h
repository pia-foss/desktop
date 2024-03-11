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

#ifndef SETTINGS_DAEMONACCOUNT_H
#define SETTINGS_DAEMONACCOUNT_H

#include "../common.h"
#include "../json.h"
#include "dedicatedip.h"
#include <unordered_set>

// Class encapsulating 'account' properties of the daemon.  This includes:
// * user's access credentials (auth token, or actual credentials if we are
//   not able to reach the API)
// * cached account information (plan, expiration date, etc., just for display)
// * other unique identifiers - not connected to the account, but that should
//   be protected from other apps (port forward token, dedicated IPs)
//
// The file storing these data is accessible only to Administrators (Windows) /
// root (Mac/Linux).
class COMMON_EXPORT DaemonAccount : public NativeJsonObject
{
    Q_OBJECT

public:
    // Several properties are sensitive (passwords, etc.) and are not needed by
    // clients.  Daemon does not send these properties to clients at all.
    static const std::unordered_set<QString> &sensitiveProperties();

public:
    DaemonAccount();

    static bool validateToken(const QString& token);

    // Convenience member to denote whether we are logged in or not.
    // While serialized, it gets reset every startup.
    JsonField(bool, loggedIn, false)

    // The PIA username
    JsonField(QString, username, {})
    // The PIA password (to be replaced with an auth token instead)
    JsonField(QString, password, {})
    // The PIA authentication token (replaces the password)
    JsonField(QString, token, {}, validateToken)

    // The following fields directly map to fetched account info.
    JsonField(QString, plan, {})
    JsonField(bool, active, false)
    JsonField(bool, canceled, false)
    JsonField(bool, recurring, false)
    JsonField(bool, needsPayment, false)
    JsonField(int, daysRemaining, 0)
    JsonField(bool, renewable, false)
    JsonField(QString, renewURL, {})
    JsonField(quint64, expirationTime, 0)
    JsonField(bool, expireAlert, false)
    JsonField(bool, expired, false)

    // These two fields denote what is passed to OpenVPN authentication.
    JsonField(QString, openvpnUsername, {})
    JsonField(QString, openvpnPassword, {})

    // Port forwarding token and signature used for port forward requests in the
    // modern infrastructure.  Collectively, these are the "port forwarding
    // token".
    JsonField(QString, portForwardPayload, {})
    JsonField(QString, portForwardSignature, {})

    // Dedicated IP tokens and the information most recently fetched from the
    // API for those tokens.  These are in DaemonAccount because the token is
    // functionally a password and should be protected like account information,
    // but they are not erased on a logout.
    JsonField(std::vector<AccountDedicatedIp>, dedicatedIps, {})
};

#endif
