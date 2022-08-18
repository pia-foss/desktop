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

#include <common/src/common.h>
#line HEADER_FILE("getcommand.h")

#ifndef GETCOMMAND_H
#define GETCOMMAND_H

#include "clicommand.h"
#include <clientlib/src/model/daemonstate.h>

// "Type" keywords used by get, set, and monitor
namespace GetSetType
{
    extern const QString connectionState, debugLogging, portForward, requestPortForward, protocol,
                         region, regions, vpnIp, pubIp, allowLAN, daemonState, daemonSettings,
                         daemonData, daemonAccount;
}

namespace GetSetValue
{
    const QString &getBooleanText(bool value);
    bool parseBooleanParam(const QString &param);
    extern const QString locationAuto;
    QString getRegionCliName(const QJsonObject &location);
    QString matchSpecifiedLocation(const DaemonState &state, const QString &location);
}

// Implements the "get" command - gets a value from daemon state/settings
// indicated by the "type" and prints.
class GetCommand : public CliCommand
{
public:
    virtual void printHelp(const QString &name) override;
    virtual int exec(const QStringList &params, QCoreApplication &app) override;
};

// "monitor" is similar to "get" - it displays specific values supported by the
// "get" command, but it remains connected and prints changes as they occur
// instead of printing the value once.
class MonitorCommand : public CliCommand
{
public:
    virtual void printHelp(const QString &name) override;
    virtual int exec(const QStringList &params, QCoreApplication &app) override;
};

class DumpCommand : public CliCommand {
public:
    virtual void printHelp(const QString &name) override;
    virtual int exec(const QStringList &params, QCoreApplication &app) override;
};

#endif
