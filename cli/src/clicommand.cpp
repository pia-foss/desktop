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
#line SOURCE_FILE("clicommand.cpp")

#include "clicommand.h"
#include "brand.h"
#include "output.h"
#include "cliclient.h"
#include "util.h"
#include <QElapsedTimer>
#include <unordered_map>

namespace
{
    // Exit code mappings.  Maps Error::Code values to a corresponding exit code
    // and (optional) message.
    // Messages are used by traceRpcError(), some codes do not need specific
    // messages since they do not occur as RPC errors.
    using ExitCodeMap = std::unordered_map<Error::Code, std::pair<CliExitCode, const char *>>;
    ExitCodeMap _exitCodes
    {
        {Error::Code::Success, {CliExitCode::Success, nullptr}},
        {Error::Code::JsonRPCConnectionLost, {CliExitCode::ConnectionLost,
            "Local RPC connection lost"}},
        {Error::Code::CliInvalidArgs, {CliExitCode::InvalidArgs, nullptr}},
        {Error::Code::CliTimeout, {CliExitCode::Timeout, nullptr}},
        // This error code is only used for RPC_connectVPN, it's also not as
        // intuitive as some of the other codes so the message is a bit more
        // specific.  We may eliminate this requirement in future versions of
        // the client.
        {Error::Code::DaemonRPCDaemonInactive, {CliExitCode::RequiresClient,
            "Cannot connect - start the " BRAND_SHORT_NAME " client to activate the daemon, or use `"\
                        BRAND_CODE "ctl background enable` to enable background mode."}},
        {Error::Code::DaemonRPCNotLoggedIn, {CliExitCode::NotLoggedIn,
            "This command requires a logged in account"}},
        {Error::Code::DaemonRPCUnknownSetting, {CliExitCode::UnknownSetting,
            "Request rejected, unknown property name"}},
    };
}

std::chrono::seconds CliTimeout::_timeout{5};

void CliTimeout::setTimeout(std::chrono::seconds timeout)
{
    _timeout = timeout;
}

CliTimeout::CliTimeout(QCoreApplication &app)
    : _app{app}
{
    _elapsed.start();
    QTimer::singleShot(msec(_timeout), Qt::TimerType::PreciseTimer, this, [this]()
    {
        std::chrono::nanoseconds nsecElapsed{_elapsed.nsecsElapsed()};
        const auto &traceElapsed = traceMsec(std::chrono::duration_cast<std::chrono::milliseconds>(nsecElapsed));
        qWarning() << "Timed out after" << traceElapsed << "- had specified"
            << traceMsec(_timeout);
        errln() << "Timed out after" << traceElapsed;
        _app.exit(CliExitCode::Timeout);
    });
}

// Map recognized Error::Code values to CliExitCode values
CliExitCode CliCommand::mapErrorCode(Error::Code code)
{
    auto itCodeData = _exitCodes.find(code);
    if(itCodeData != _exitCodes.end())
        return itCodeData->second.first;
    return CliExitCode::OtherError;
}

Error::Code CliCommand::mapExitCode(CliExitCode code)
{
    // This is O(N), but it's rarely used.
    for(const auto &codeData : _exitCodes)
    {
        if(codeData.second.first == code)
            return codeData.first;
    }
    return Error::Code::Unknown;
}

int CliCommand::traceRpcError(const Error &error)
{
    qWarning() << "RPC received error:" << error;
    if(error.code() == Error::Code::Success)
    {
        qWarning() << "Unexpected error result with no error code:" << error;
        errln() << "Request failed";
        // Don't exit with an unexpected success code
        return CliExitCode::OtherError;
    }

    auto itCodeData = _exitCodes.find(error.code());
    const char *message = itCodeData != _exitCodes.end() ? itCodeData->second.second : nullptr;
    // Print a specific message if this code has one, otherwise print the error
    if(message)
        errln() << message;
    else
        errln() << "Request failed, error:" << error;

    // Exit with the corresponding exit code
    return itCodeData != _exitCodes.end() ? itCodeData->second.first : CliExitCode::OtherError;
}

void CliCommand::checkNoParams(const QStringList &params)
{
    if(params.length() > 1)
    {
        errln() << "Unexpected parameters for command:" << params[0];
        throw Error{HERE, Error::Code::CliInvalidArgs};
    }
}

QJsonValue CliCommand::execOneShot(QCoreApplication &app,
                                   const QString &rpcMethod,
                                   const QJsonArray &rpcArgs)
{
    CliClient client;

    // We don't have to reference this later, just hang onto the Async here so
    // it's kept alive until either it completes or we abort.
    Async<void> daemonRpcResult;
    QJsonValue rpcValue;

    QObject localConnState{};

    // Start a timer to exit after the timeout elapses
    CliTimeout timeout{app};

    QObject::connect(&client, &CliClient::firstConnected, &localConnState, [&]()
    {
        daemonRpcResult = client.connection().call(rpcMethod, rpcArgs)
            ->next(&localConnState, [&](const Error &error, const QJsonValue &result)
            {
                if(error)
                {
                    // Qt isn't exception safe, so we can't throw through
                    // QCoreApplication::exec().  Instead, exit with an exit
                    // code, which is then re-thrown below.  This loses the
                    // specific code for errors that map to
                    // CliExitCode::OtherError, but that doesn't affect the exit
                    // status.
                    app.exit(traceRpcError(error));
                }
                else
                {
                    // Success
                    qInfo() << "Daemon accepted" << rpcMethod << "RPC";
                    rpcValue = result;
                    app.exit(CliExitCode::Success);
                }
            });
    });

    auto exitCode = app.exec();
    // If the request failed, throw; there is no JSON result to return
    if(exitCode != Error::Code::Success)
        throw Error{HERE, mapExitCode(static_cast<CliExitCode>(exitCode))};
    return rpcValue;
}

void TrivialRpcCommand::printHelp(const QString &)
{
    outln() << _description;
}

int TrivialRpcCommand::exec(const QStringList &params, QCoreApplication &app)
{
    checkNoParams(params);
    // Don't care about the JSON result, nothing meaningful for trivial RPCs
    // like connect/disconnect/etc.
    execOneShot(app, _method, {});
    return CliExitCode::Success;
}
