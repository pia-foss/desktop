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
#line SOURCE_FILE("makecommand.cpp")

#include "makecommand.h"
#include "output.h"
#include "applysettings.h"
#include "getcommand.h"
#include "setcommand.h"
#include "watchcommand.h"
#include "brand.h"

const QString connectDescription =
    QStringLiteral(
        "Connects to the VPN, or reconnects to apply new settings.\n"
        "The " BRAND_SHORT_NAME " client must be running to use this command.\n"
        "(The " BRAND_SHORT_NAME " daemon is inactive when the client is not running.)");
const QString disconnectDescription =
    QStringLiteral("Disconnects from the VPN.");
const QString resetSettingsDescription =
    QStringLiteral("Resets daemon settings to the defaults (ports/protocols/etc.)\nClient settings (themes/icons/layouts) can't be set with the CLI.");
const QString checkDriverDescription =
    QStringLiteral("Hint to the daemon to re-check the driver installation states.");

using CommandMap = std::map<QString, std::shared_ptr<CliCommand>>;
const CommandMap stableCommands
{
    {"connect", std::make_shared<TrivialRpcCommand>("connectVPN", connectDescription)},
    {"disconnect", std::make_shared<TrivialRpcCommand>("disconnectVPN", disconnectDescription)},
    {"get", std::make_shared<GetCommand>()},
    {"monitor", std::make_shared<MonitorCommand>()},
    {"resetsettings", std::make_shared<TrivialRpcCommand>("resetSettings", resetSettingsDescription)},
    {"set", std::make_shared<SetCommand>()}
};

const CommandMap unstableCommands
{
    {"applysettings", std::make_shared<ApplySettingsCommand>()},
#ifdef Q_OS_WIN
    // Windows only - used to hint to the daemon to re-check driver installation
    // states that do not provide change notifications.
    //
    // WinTUN is checked this way, there does not appear to be any way to be
    // notified when an MSI package or driver is installed.
    //
    // The WFP callout is checked this way on Win 10 1507, because using SCM
    // notifications on this build of Windows for a driver service cause SCM to
    // crash on shutdown.
    {"checkdriver", std::make_shared<TrivialRpcCommand>("checkDriverState", checkDriverDescription)},
#endif
    {"watch", std::make_shared<WatchCommand>()}
};

CliCommand *getCommandFromMap(const CommandMap &commands, const QString &name)
{
    auto itCommand = commands.find(name);
    if(itCommand != commands.end())
        return itCommand->second.get();
    return nullptr;
}

void printCommandsFromMap(const CommandMap &commands)
{
    OutputIndent commandIndent{2};
    for(const auto &commandEntry : commands)
    {
        outln() << commandEntry.first;

        OutputIndent bodyIndent{2};
        Q_ASSERT(commandEntry.second);  // Invariant, no nullptrs in command maps
        commandEntry.second->printHelp(commandEntry.first);
        outln();
    }
}

void printCommandsHelp(bool allowUnstable)
{
    if(allowUnstable)
        outln() << "Stable commands:";
    else
        outln() << "Commands:";

    printCommandsFromMap(stableCommands);

    if(allowUnstable)
    {
        outln() << "Unstable commands:";
        outln() << "**These commands may be changed or removed in future versions.**";
        printCommandsFromMap(unstableCommands);
    }
}

CliCommand &getCommand(bool allowUnstable, const QString &name)
{
    CliCommand *pCommand = getCommandFromMap(stableCommands, name);
    // If unstable commands are enabled, also check those.
    if(allowUnstable && !pCommand)
        pCommand = getCommandFromMap(unstableCommands, name);

    if(!pCommand)
    {
        errln() << "Unknown command:" << name;
        throw Error{HERE, Error::Code::CliInvalidArgs};
    }
    return *pCommand;
}
