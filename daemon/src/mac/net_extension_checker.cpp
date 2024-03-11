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

#include "net_extension_checker.h"
NetExtensionChecker::NetExtensionChecker(std::string transparentProxyCliExecutable,
                    std::chrono::milliseconds shortInterval, std::chrono::milliseconds longInterval)
: _transparentProxyCliExecutable{transparentProxyCliExecutable}
, _shortInterval{shortInterval}
, _longInterval{longInterval}
{}

StateModel::NetExtensionState NetExtensionChecker::checkInstallationState()
{
    qInfo() << "Checking MacOS Network Extension Status";

    if(isInstalled())
        return StateModel::NetExtensionState::Installed;
    else
        return StateModel::NetExtensionState::NotInstalled;
}

void NetExtensionChecker::start(StateModel::NetExtensionState installState, bool isEnabled)
{
    qInfo() << "MacOS Network Extension Checker start";
    _lastState = installState;
    connect(&_timer, &QTimer::timeout, this,
                &NetExtensionChecker::checkIfStateChanged);
    updateTimer(installState, isEnabled);
    _timer.start();
}

void NetExtensionChecker::updateTimer(StateModel::NetExtensionState installState, bool isEnabled)
{
    std::chrono::milliseconds newInterval = _longInterval;
    if(installState != StateModel::NetExtensionState::Installed)
    {
        if(isEnabled)
        {
            qDebug() << "MacOS Network Extension is not installed and Split Tunnel is enabled. Timer set to short interval";
            newInterval = _shortInterval;
        }
    }
    _timer.setInterval(newInterval);
}

void NetExtensionChecker::checkIfStateChanged()
{
    auto state = checkInstallationState();
    if(state != _lastState)
    {
        qInfo() << "MacOS Network Extension installation state has changed to: " << qEnumToString(state);
        _lastState = state;
        emit stateChanged(state);
    }
}

bool NetExtensionChecker::isNetExtensionInstalled()
{
    std::string sysextStatus = kapps::core::Exec::cmdWithOutput(_transparentProxyCliExecutable, {"sysext", "status"});
    // We accept two possible states here:
    // - bundled installed: the installed extension is the same version as the bundle one
    // - other installed: an older version is installed
    if(sysextStatus.find("bundled installed") != std::string::npos ||
       sysextStatus.find("other installed") != std::string::npos)
        return true;
    else
        return false;
}

bool NetExtensionChecker::isProxyInstalled()
{
    std::string sysextStatus = kapps::core::Exec::cmdWithOutput(_transparentProxyCliExecutable, {"proxy", "status"});
    // Any status is accepted, except for uninstalled or invalid
    if(sysextStatus.find("uninstalled") == std::string::npos && sysextStatus.find("invalid") == std::string::npos)
        return true;
    else
        return false;
}

bool NetExtensionChecker::isInstalled()
{
    bool netExtensionInstalled = isNetExtensionInstalled();
    bool proxyInstalled = isProxyInstalled();
    return netExtensionInstalled && proxyInstalled;
}
