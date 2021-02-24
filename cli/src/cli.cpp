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
#line SOURCE_FILE("cli.cpp")

#include "clientlib.h"
#include "output.h"
#include "path.h"
#include "util.h"
#include "version.h"
#include "makecommand.h"
#include <QCommandLineParser>

int cliMain(int argc, char *argv[])
{
    Path::initializePreApp();
    // Qt supports a handful of built-in options, none of which are meaningful
    // to pia-cli.  Pass dummy argc/argv values to QCoreApplication so there
    // won't be any conflicts with our own command-line options.
    int dummyArgc = 1;
    char *dummyArgv[2]{argv[0], nullptr};
    QCoreApplication app{dummyArgc, dummyArgv};
    Path::initializePostApp();

    Logger logSingleton{Path::CliLogFile};

    QCommandLineParser parser;
    parser.addOptions({
        {QStringList{"timeout", "t"}, "Sets timeout for one-shot commands.", "seconds", "5"},
        {QStringList{"debug", "d"}, "Prints debug logs to stderr."},
        // Use our own "help" option rather than the built-in one - Qt stops
        // parsing when it encounters the built-in option, we need it to keep
        // parsing in case --unstable was given too.
        {QStringList{"help", "h"}, "Displays this help."}
    });
    // Add the hidden "--unstable" option
    QCommandLineOption unstableOption{QStringList{"unstable", "u"}, ""};
    unstableOption.setFlags(QCommandLineOption::Flag::HiddenFromHelp);
    parser.addOption(unstableOption);
    // Version option
    parser.addVersionOption();
    // Positional arguments (for the CLI, command and parameter(s))
    parser.addPositionalArgument("command", "Command to execute");
    parser.addPositionalArgument("parameters", "Parameters for the command", "[parameters...]");

    parser.setApplicationDescription("Command-line interface to the PIA client.  Some commands, such as connect, require that the graphical client is also running.");

    QStringList args;
    args.reserve(argc);
    for(int i=0; i<argc && argv[i]; ++i)
        args.push_back(QString::fromLocal8Bit(argv[i]));

    // Use parse() instead of process() so we can add on more text for --help.
    if(!parser.parse(args))
    {
        errln() << parser.errorText();
        return CliExitCode::InvalidArgs;
    }

    auto printHelp = [&]()
    {
        outln() << parser.helpText();
        printCommandsHelp(parser.isSet("unstable"));
    };

    // Check special options
    if(parser.isSet("help"))
    {
        printHelp();
        return CliExitCode::Success;
    }

    if(parser.isSet("version"))
    {
        outln() << PIA_VERSION;
        return CliExitCode::Success;
    }

    // Check for regular options
    if(parser.isSet("debug"))
        Logger::enableStdErr(true);
    QString timeoutStr = parser.value(QStringLiteral("timeout"));
    auto timeoutSec = timeoutStr.toInt();
    if(timeoutSec <= 0)
    {
        errln() << "Invalid timeout value:" << timeoutStr;
        return CliExitCode::InvalidArgs;
    }
    CliTimeout::setTimeout(std::chrono::seconds{timeoutSec});

    auto cmdArgs = parser.positionalArguments();
    if(cmdArgs.empty())
    {
        errln() << "No command specified";
        printHelp();
        return CliExitCode::InvalidArgs;
    }

    try
    {
        auto &command = getCommand(parser.isSet("unstable"), cmdArgs.front());
        return command.exec(cmdArgs, app);
    }
    catch(const Error &err)
    {
        auto exitCode = CliCommand::mapErrorCode(err.code());
        qInfo() << "CLI error" << err << "-> exit code"<< exitCode;
        return exitCode;
    }
}

int main(int argc, char **argv)
{
    // The CLI doesn't log to stderr by default (the --debug option can enable
    // stderr logging)
    return runClient(false, argc, argv, &cliMain);
}
