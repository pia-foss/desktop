// Copyright (c) 2020 Private Internet Access, Inc.
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
#line SOURCE_FILE("applysettings.cpp")

#include "applysettings.h"
#include "output.h"
#include <QJsonDocument>

void ApplySettingsCommand::printHelp(const QString &name)
{
    outln() << "usage:" << name << "<settings-json>";
    outln() << "Apply settings values from a JSON object.";
    outln() << "Can be used to set daemon settings (ports/protocols/etc.).";
    outln() << "Client settings (themes/icons/layouts) can't be set with the CLI.";
}

int ApplySettingsCommand::exec(const QStringList &params, QCoreApplication &app)
{
    if(params.length() != 2)
    {
        errln() << "usage:" << params[0] << "<settings-json>";
        throw Error{HERE, Error::Code::CliInvalidArgs};
    }

    // Parse the JSON payload and build the RPC arguments
    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(params[1].toUtf8(), &parseError);
    if(doc.isNull())
    {
        qWarning() << "JSON parse error:" << parseError.errorString();
        qWarning() << "Unparseable JSON:" << params[1];
        errln() << "Invalid JSON:" << parseError.errorString();
        throw Error{HERE, Error::Code::CliInvalidArgs};
    }

    if(!doc.isObject())
    {
        qWarning() << "JSON is not object:" << params[1];
        errln() << "JSON payload must be an object";
        throw Error{HERE, Error::Code::CliInvalidArgs};
    }

    QJsonArray rpcArgs{doc.object()};
    execOneShot(app, QStringLiteral("applySettings"), rpcArgs);
    return CliExitCode::Success;
}
