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

#pragma once
#include "../wireguardbackend.h"
#include <QProcess>
#include <common/src/async.h>
#include <memory>

// Task to execute wgservice.exe /getconfig and parse the JSON output
// This replaces the pipe-based communication with CLI-based status retrieval
//
// Usage:
//   auto task = Async<WgServiceCliTask>::create("wgpia0");
//   task->then([](const std::shared_ptr<WgDevStatus> &pDev) {
//       // Use the parsed WireGuard device status
//   });
//
// The task will:
// 1. Execute: wgservice.exe /getconfig <interfaceName>
// 2. Parse the JSON output into WgDevStatus
// 3. Resolve with the parsed status, or reject on error
//
// Error scenarios:
// - wgservice.exe not found → WireguardNotResponding
// - Process crashes → WireguardNotResponding
// - Non-zero exit code → WireguardNotResponding
// - Empty/malformed JSON → WireguardNotResponding
// - Timeout → WireguardNotResponding
class WgServiceCliTask : public Task<std::shared_ptr<WgDevStatus>>
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("wgserviceclitask")

public:
    // Create a task to retrieve WireGuard status for the given interface
    // The command will be executed immediately upon construction
    explicit WgServiceCliTask(QString interfaceName);
    ~WgServiceCliTask();

private:
    // Execute the wgservice.exe command and set up async handlers
    void executeCommand();

private:
    QString _interfaceName;
    std::unique_ptr<QProcess> _process;
};
