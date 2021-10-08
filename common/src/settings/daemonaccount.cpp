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
#include "daemonaccount.h"

const std::unordered_set<QString> &DaemonAccount::sensitiveProperties()
{
    static const std::unordered_set<QString> _sensitiveProperties
    {
        QStringLiteral("password"),
        QStringLiteral("token"),
        QStringLiteral("openvpnUsername"),
        QStringLiteral("openvpnPassword"),
        QStringLiteral("clientId"),
        QStringLiteral("portForwardPayload"),
        QStringLiteral("portForwardSignature"),
        // Clients don't need the dedicated IPs object at all, they observe
        // these through the DIP regions.  This protects the tokens (the other
        // information is present in the DIP regions).
        QStringLiteral("dedicatedIps")
    };
    return _sensitiveProperties;
}

DaemonAccount::DaemonAccount()
    : NativeJsonObject(DiscardUnknownProperties)
{

}

bool DaemonAccount::validateToken(const QString& token)
{
    static const QRegularExpression validToken(QStringLiteral("^[0-9A-Fa-f]+$"));
    return token.isEmpty() || validToken.match(token).hasMatch();
}
