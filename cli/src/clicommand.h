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

#include <common/src/common.h>
#line HEADER_FILE("clicommand.h")

#ifndef CLICOMMAND_H
#define CLICOMMAND_H

#include <QJsonArray>
#include <QElapsedTimer>
#include <chrono>

// Exit codes that can be returned from the CLI app.  Exit codes are limited to
// the range 0-127, so we can't return any arbitrary Error::Code value.
//
// Most Error::Code::DaemonRPC* values have a specific exit code, and some other
// errors also have codes.
enum CliExitCode : int
{
    Success,
    InvalidArgs,
    Timeout,
    ConnectionLost,
    RequiresClient,
    NotLoggedIn,
    UnknownSetting,
    DedicatedIpTokenExpired,
    DedicatedIpTokenInvalid,
    OtherError = 127,
};

// Map recognized Error::Code values to CliExitCode values
CliExitCode cliMapErrorCode(Error::Code code);
// Map a CliExitCode back to an Error::Code (used occasionally when a
// CliExitCode must be re-thrown as an Error)
Error::Code cliMapExitCode(CliExitCode code);

// Timeout used by one-shot CLI commands to exit after the specified timeout.
class CliTimeout : public QObject
{
    Q_OBJECT
private:
    static std::chrono::seconds _timeout;
public:
    // Set the timeout specified by the user on the command line.  This can't be
    // used once any CliTimeouts have been created.
    static void setTimeout(std::chrono::seconds timeout);

public:
    CliTimeout(QCoreApplication &app);

private:
    QCoreApplication &_app;
    // Used to print the elapsed time if a timeout occurs
    QElapsedTimer _elapsed;
};

// Model of any CLI command.  CliCommands can throw an Error during exec() to
// exit the CLI with a specific exit code.
class CliCommand
{
public:
    // Get the exit code corresponding to an Error::Code
    static CliExitCode mapErrorCode(Error::Code code);
    // Get an Error::Code that corresponds to an exit code.
    //
    // CliCommand ensures that mapErrorCode(mapExitCode(exitCode)) returns the
    // original exit code, but the Error::Code returned here might not be the
    // original error code that generated this exit code.
    //
    // This typically only used to work around the lack of exception safety in
    // QCoreApplication::exec() (can't throw through it, instead exit, re-map
    // an error code and re-throw), which works.
    static Error::Code mapExitCode(CliExitCode code);
    // Trace and print an error received from an RPC.  Returns an appropriate
    // exit code (always nonzero; Success is unexpected here).
    static int traceRpcError(const Error &error);

public:
    virtual ~CliCommand() = default;

protected:
    // Check params in exec() for a command that takes no parameters (throws if
    // any parameters are given)
    void checkNoParams(const QStringList &params);

    // Execute a one-shot command.  Connects to the daemon, then issues a daemon
    // RPC with the name and arguments specified.  If the RPC completes
    // successfully, this returns the QJsonValue returned by the daemon - the
    // caller can print this to stdout if needed.  If the RPC does not complete,
    // this prints diagnostics if needed and throws an Error.
    QJsonValue execOneShot(QCoreApplication &app, const QString &rpcMethod,
                           const QJsonArray &rpcArgs);

public:
    // Print the command's help text.
    virtual void printHelp(const QString &name) = 0;

    // Execute the command.  'params' includes the command name and any
    // additional parameters from the command line.  The return value becomes
    // the application's exit code.
    //
    // params always contains at least the command name (ensured by
    // makeCommand() / cliMain().)
    virtual int exec(const QStringList &params, QCoreApplication &app) = 0;
};

// Model of a trivial RPC command with no params and no result
class TrivialRpcCommand : public CliCommand
{
public:
    TrivialRpcCommand(QString method, QString description)
        : _method{std::move(method)}, _description{std::move(description)}
    {}
public:
    virtual void printHelp(const QString &name) override;
    virtual int exec(const QStringList &params, QCoreApplication &app) override;
private:
    QString _method, _description;
};

#endif
