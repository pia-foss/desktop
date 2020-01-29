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

#ifndef TASKS_PROCESS_H
#define TASKS_PROCESS_H
#pragma once

#include "tasks.h"

// Determine if a process with a given filename (path is ignored) is running
bool isProcessRunning(utf16ptr executable);

// Task to kill any running client processes.
class KillExistingClientsTask : public Task
{
public:
    using Task::Task;
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 1.0; }
    virtual double getEstimatedRollbackTime() const override { return 0.0; }
};

#endif // TASKS_PROCESS_H
