// Copyright (c) 2026 Private Internet Access, Inc.
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
                                         std::chrono::milliseconds shortInterval,
                                         std::chrono::milliseconds longInterval,
                                         int maxActivationAttempts)
: _transparentProxyCliExecutable{transparentProxyCliExecutable}
, _shortInterval{shortInterval}
, _longInterval{longInterval}
, _maxActivationAttempts{maxActivationAttempts}
, _attempts{0}
{
    // Creating the timer, but not starting it
    connect(&_timer, &QTimer::timeout, this,
            &NetExtensionChecker::checkIfNetExtensionStateChanged);
}

void NetExtensionChecker::start(StateModel::NetExtensionState installState)
{
    if(!_timer.isActive())
    {
        qInfo() << "Starting MacOS Network Extension Checker. ";
        _lastState = installState;
        // Every start is a fresh activation attempt, so the user gets the full
        // budget again rather than whatever was left over from the last one.
        _attempts = 0;
        updateTimer(installState);
        _timer.start();
    }
}

void NetExtensionChecker::stop()
{
    if(_timer.isActive())
    {
        qInfo() << "Stopping MacOS Network Extension Checker";
        _timer.stop();
    }
}

void NetExtensionChecker::updateTimer(StateModel::NetExtensionState installState)
{
    if(installState == StateModel::NetExtensionState::FailedActivation)
        return;

    std::chrono::milliseconds newInterval = _longInterval;
    if(installState != StateModel::NetExtensionState::Installed)
    {
        qDebug() << "MacOS Network Extension is not installed and Split Tunnel is enabled. Timer set to short interval";
        newInterval = _shortInterval;
    }
    _timer.setInterval(newInterval);
}

StateModel::NetExtensionState NetExtensionChecker::checkInstallationState() const
{
    qDebug() << "Checking MacOS Network Extension Status";
    if(isInstalled())
        return StateModel::NetExtensionState::Installed;
    else
        return StateModel::NetExtensionState::NotInstalled;
}

// private methods

void NetExtensionChecker::checkIfNetExtensionStateChanged()
{
    auto currentState = checkInstallationState();
    if(currentState == StateModel::NetExtensionState::NotInstalled)
    {
        _attempts++;
        // The user failed to comply with the activation instructions, or the cat activated split
        // tunnel
        if(_attempts >= _maxActivationAttempts)
        {
            qInfo() << "Reached maximum number of attempts for system extension activation";
            _attempts = 0;
            emit stateChanged(StateModel::NetExtensionState::FailedActivation);
            return;
        }
    }
    // This can change from not installed to installed -> good path.
    // Or it can change from installed to not installed -> this means the user has manually removed
    // the proxy from the system likely using system settings.
    if(currentState != _lastState)
    {
        qInfo() << "MacOS Network Extension installation state has changed from:" <<qEnumToString(_lastState) << "to:" << qEnumToString(currentState);
        _lastState = currentState;
        emit stateChanged(currentState);
    }
}

bool NetExtensionChecker::isInstalled() const
{
    bool netExtensionInstalled = isNetExtensionInstalled();
    bool proxyInstalled = isProxyInstalled();
    return netExtensionInstalled && proxyInstalled;
}

bool NetExtensionChecker::isNetExtensionInstalled() const
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

bool NetExtensionChecker::isProxyInstalled() const
{
    std::string sysextStatus = kapps::core::Exec::cmdWithOutput(_transparentProxyCliExecutable, {"proxy", "status"});
    // Any status is accepted, except for uninstalled or invalid
    if(sysextStatus.find("uninstalled") == std::string::npos && sysextStatus.find("invalid") == std::string::npos)
        return true;
    else
        return false;
}
