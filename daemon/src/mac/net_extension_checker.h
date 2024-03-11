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

#pragma once
#include <common/src/common.h>
#include <kapps_core/src/newexec.h>
#include <string>
#include <chrono>
#include "../model/state.h"
#include <QTimer>


class NetExtensionChecker : public QObject {
    Q_OBJECT

public:
    NetExtensionChecker(std::string transparentProxyCliExecutable,
                        std::chrono::milliseconds shortInterval, std::chrono::milliseconds longInterval);

    StateModel::NetExtensionState checkInstallationState();

    // Starting the service that will periodically (forever) check for
    // network extension installation state.
    // We do not stop checking even after the extension has been installed
    // because it could be uninstalled (in multiple ways) by the user.
    // If that happens, we want to provide feedback and update the UI,
    // without having to restart the daemon
    void start(StateModel::NetExtensionState installState, bool isEnabled);

    // Timer is set to long interval in every scenario, except when
    // Split Tunnel is enabled and the extension is not yet installed.
    // In the latter, we check faster to give a quicker user feedback
    void updateTimer(StateModel::NetExtensionState installState, bool isEnabled);

private:
    void checkIfStateChanged();

    // Check that the network extension is installed in the system.
    // This requires the users to go into Privacy & Security settings and allow it
    bool isNetExtensionInstalled();

    // Check that the proxy manager is installed in Filter & Proxies
    bool isProxyInstalled();

    bool isInstalled();

private:
    QTimer _timer;
    std::string _transparentProxyCliExecutable;
    std::chrono::milliseconds _shortInterval;
    std::chrono::milliseconds _longInterval;
    StateModel::NetExtensionState _lastState;

signals:
    void stateChanged(StateModel::NetExtensionState state);
};
