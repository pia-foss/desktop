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

#include "common.h"
#line SOURCE_FILE("backgroundcommand.cpp")

#include "backgroundcommand.h"
#include "setcommand.h"
#include "output.h"
#include "cliclient.h"
#include "brand.h"



void BackgroundCommand::printHelp(const QString &name)
{
    outln() << "usage:" << name << "<enable|disable>";
    outln() << "Allow the killswitch and/or VPN connection to remain active in the background when the GUI client is not running.";
    outln() << "When enabled, the " BRAND_SHORT_NAME " daemon will stay active even if the GUI client is closed or has not been started.";
    outln() << "This allows `" BRAND_CODE "ctl connect` to be used even if the GUI client is not running.";
    outln() << "Disabling background activation will disconnect the VPN and deactivate killswitch if the GUI client is not running.";
}

int BackgroundCommand::exec(const QStringList &params, QCoreApplication &app)
{
    if(params.length() != 2) {
        errln() << "Usage:" << params[0] << "<enable|disable>";
        throw Error{HERE, Error::Code::CliInvalidArgs};
    }

    // desired result of the command
    bool result = false;
    if(params[1] == "enable") {
        result = true;
    } else if (params[1] == "disable") {
        result = false;
    } else {
        errln() << "Usage:" << params[0] << "<enable|disable>";
        throw Error{HERE, Error::Code::CliInvalidArgs};
    }

    QJsonObject settings;
    settings.insert(QStringLiteral("persistDaemon"), result);
    QJsonArray rpcArgs{settings};
    execOneShot(app, QStringLiteral("applySettings"), rpcArgs);
    return CliExitCode::Success;
}
