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

#include "authcommand.h"
#include "output.h"
#include "cliclient.h"
#include "settings.h"
#include "brand.h"

namespace
{
    QString trimEndCR(QString target)
    {
        if(target.endsWith(QStringLiteral("\r")))
            target.chop(1);
        return target;
    }
}

QStringList readCredentialFile(const QString &fileName)
{
    QFile credsFile(fileName);
    if(!credsFile.open(QIODevice::ReadOnly))
    {
        errln() << "Could not open file: " << fileName;
        throw Error{HERE, Error::Code::CliInvalidArgs};
    }

    // Read a max of 1000 bytes to prevent loading very large file into memory
    QString credsFileContent = credsFile.read(1000);
    QStringList parts = credsFileContent.split("\n");

    for(int i=0; i<parts.size(); ++i)
    {
        const auto &valueTrimmed = trimEndCR(parts[i]);
        parts[i] = valueTrimmed;
    }

    return parts;
}

void LoginCommand::printHelp(const QString &name)
{
    outln() << "usage:" << name << "<login_file>";
    outln() << "Log in to your " BRAND_SHORT_NAME " account.";
    outln() << "Put your username and password on separate lines in a text file,";
    outln() << "and specify that file on the command line:";
    outln() << "    p0000000";
    outln() << "    (yourpassword)";
}

int LoginCommand::exec(const QStringList &params, QCoreApplication &app)
{
    CliClient client;

    if(params.size() < 2) {
        errln() << "Missing parameter: <login_file>";
        return CliExitCode::InvalidArgs;
    }

    QStringList parts = readCredentialFile(params[1]);
    if(parts.count() < 2) {
        errln () << "Invalid file format.";
        return CliExitCode::InvalidArgs;
    }

    QObject localConnState{};

    // Start a timer to exit after the timeout elapses
    CliTimeout timeout{app};

    QObject::connect(&client, &CliClient::firstConnected, &localConnState, [&]()
    {
        if(client.connection().account.loggedIn()) {
            errln () << "Already logged into account " << client.connection().account.username();
            app.exit(CliExitCode::OtherError);
        }

        client.connection().call("login", QJsonArray{parts[0], parts[1]})
                        ->notify(&localConnState, [&](const Error &error, const QJsonValue &result)
        {
            Q_UNUSED(result);
            if(error)
            {
                // Qt isn't exception safe, so we can't throw through
                // QCoreApplication::exec().  Instead, exit with an exit
                // code, which is then re-thrown below.  This loses the
                // specific code for errors that map to
                // CliExitCode::OtherError, but that doesn't affect the exit
                // status.
                errln () << "Unable to log in.";
                app.exit(traceRpcError(error));
            }
            else
            {
                app.exit(CliExitCode::Success);
            }
        });
    });

    auto exitCode = app.exec();
    // If the request failed, throw; there is no JSON result to return
    if(exitCode != Error::Code::Success)
        throw Error{HERE, mapExitCode(static_cast<CliExitCode>(exitCode))};

    return CliExitCode::Success;
}
