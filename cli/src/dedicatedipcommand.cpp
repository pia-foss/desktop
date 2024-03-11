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

#include "dedicatedipcommand.h"
#include <common/src/output.h>
#include "cliclient.h"
#include "authcommand.h"  // Shared functionality to load creds file
#include "brand.h"
#include "getcommand.h"

void DedicatedIpCommand::printHelp(const QString &name)
{
    outln() << "usage (add):" << name << "add <token_file>";
    outln() << "usage (remove):" << name << "remove <region_id>";
    outln() << "Add or remove a Dedicated IP.";
    outln() << "To add, put the dedicated IP token in a text file (by itself), and specify that file on the command line:";
    outln() << "    DIP20000000000000000000000000000";
    outln() << "(This ensures the token is not visible in the process command line or environment.)";
    outln() << "To remove, specify the dedicated IP region ID, as shown by `" BRAND_CODE "ctl get regions`, such as";
    outln() << "`dedicated-sweden-000.000.000.000`.";
}

int DedicatedIpCommand::exec(const QStringList &params, QCoreApplication &app)
{
    if(params.size() < 2)
    {
        errln() << "Missing parameter: add/remove";
        return CliExitCode::InvalidArgs;
    }

    if(params[1] == QStringLiteral("add"))
    {
        return execAdd(params, app);
    }
    else if(params[1] == QStringLiteral("remove"))
    {
        return execRemove(params, app);
    }
    else
    {
        errln() << "Unknown action" << params[1] << "- expected add or remove";
        return CliExitCode::InvalidArgs;
    }
}

int DedicatedIpCommand::execAdd(const QStringList &params, QCoreApplication &app)
{
    if(params.size() < 3)
    {
        errln() << "Missing parameter: <token_file>";
        return CliExitCode::InvalidArgs;
    }

    QStringList tokenCreds = readCredentialFile(params[2]);
    if(tokenCreds.count() < 1)
    {
        errln() << "Invalid file format.";
        return CliExitCode::InvalidArgs;
    }

    execOneShot(app, QStringLiteral("addDedicatedIp"), QJsonArray{tokenCreds[0]});
    return CliExitCode::Success;
}

int DedicatedIpCommand::execRemove(const QStringList &params, QCoreApplication &app)
{
    if(params.size() < 3)
    {
        errln() << "Missing parameter: <region_id>";
        return CliExitCode::InvalidArgs;
    }

    // Like "set", we can't implement this with execOneShot because we need the
    // daemon state to determine the real ID of the specified region.
    CliClient client;
    CliTimeout timeout{app};
    QObject localConnState{};
    Async<void> removeResult;

    QObject::connect(&client, &CliClient::firstConnected, &localConnState, [&]()
    {
        try
        {
            const auto &regionId = GetSetValue::matchSpecifiedLocation(client.connection().state, params[2]);
            removeResult = client.connection().call(QStringLiteral("removeDedicatedIp"),
                                                    QJsonArray{regionId})
                ->next(&localConnState, [&app, &params, this](const Error &error, const QJsonValue &)
                {
                    if(error)
                    {
                        app.exit(traceRpcError(error));
                    }
                    else
                    {
                        qInfo() << "Remove RPC completed for dedicated IP"
                            << params[2];
                        app.exit(CliExitCode::Success);
                    }
                });
        }
        catch(const Error &error)
        {
            qWarning() << "Unable to remove dedicated IP:" << error;
            app.exit(mapErrorCode(error.code()));
        }
    });

    return app.exec();
}
