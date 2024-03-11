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

#ifndef SETTINGS_SPLITTUNNEL_H
#define SETTINGS_SPLITTUNNEL_H

#include "../common.h"
#include "../json.h"
#include <QHostAddress>
#include <QAbstractSocket>

class COMMON_EXPORT SplitTunnelRule : public NativeJsonObject
{
    Q_OBJECT

public:
    SplitTunnelRule();
    SplitTunnelRule(const SplitTunnelRule &other) {*this = other;}
    SplitTunnelRule &operator=(const SplitTunnelRule &other)
    {
        path(other.path());
        linkTarget(other.linkTarget());
        mode(other.mode());
        return *this;
    }
    SplitTunnelRule(const QString &_path, const QString &_linkTarget, const QString &_mode) {
        path(_path);
        linkTarget(_linkTarget);
        mode(_mode);
    }

    bool operator==(const SplitTunnelRule &other) const
    {
        return mode() == other.mode() && linkTarget() == other.linkTarget() && path() == other.path();
    }
    bool operator!=(const SplitTunnelRule &other) const {return !(*this == other);}

    // Path to the item selected for this rule.  The selection made by the user
    // is stored directly.  For example:
    // - on Windows this may be a Start menu shortcut (enumerated) or path to an
    //   executable (browsed manually)
    // - on Mac it is a path to an app bundle (either enumerated or browsed
    //   manually)
    // - on Linux it is a path to an executable (browsed manually)
    //
    // On most platforms we have to do additional work to figure out what
    // exactly will be used for the rule (follow links, enumerate helper
    // executables, etc.)  That's done when the firewall rules are applied,
    // rather than when the selection is made, so we can update that logic in
    // the future.
    JsonField(QString, path, {})
    // On Windows only, if the path identifies a shortcut, target is the target
    // of that shortcut as resolved by the user that added the rule.
    //
    // Many apps on Windows install both their shortcut and executable to the
    // user's local AppData, which would prevent the daemon from resolving the
    // link (even with SLGP_RAWPATH, the system still seems to remap it to the
    // service account's AppData somehow).
    //
    // This field isn't used for manually-browsed executables, and it isn't used
    // on any other platform.
    JsonField(QString, linkTarget, {})
    // The behavior to apply for this rule.  The terminology is historical:
    // - "exclude" = Bypass VPN (exclude from VPN)
    // - "include" = Only VPN
    JsonField(QString, mode, QStringLiteral("exclude"), { "exclude", "include" })
};

class COMMON_EXPORT SplitTunnelSubnetRule : public NativeJsonObject
{
    Q_OBJECT

public:
    SplitTunnelSubnetRule();
    SplitTunnelSubnetRule(const SplitTunnelSubnetRule &other) {*this = other;}
    SplitTunnelSubnetRule &operator=(const SplitTunnelSubnetRule &other)
    {
        subnet(other.subnet());
        mode(other.mode());
        return *this;
    }

    bool operator==(const SplitTunnelSubnetRule &other) const
    {
        return mode() == other.mode() && subnet() == other.subnet();
    }
    bool operator!=(const SplitTunnelSubnetRule &other) const {return !(*this == other);}

    QString normalizedSubnet() const
    {
        auto subnetPair = QHostAddress::parseSubnet(subnet());
        return subnetPair.first.isNull() ? "" : QStringLiteral("%1/%2").arg(subnetPair.first.toString()).arg(subnetPair.second);
    }

    QAbstractSocket::NetworkLayerProtocol protocol() const { return QHostAddress::parseSubnet(subnet()).first.protocol(); }

    JsonField(QString, subnet, {})
    JsonField(QString, mode, QStringLiteral("exclude"))
};


#endif
