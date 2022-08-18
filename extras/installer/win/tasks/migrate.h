// Copyright (c) 2022 Private Internet Access, Inc.
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

#ifndef TASKS_MIGRATE_H
#define TASKS_MIGRATE_H
#pragma once

#include "../tasks.h"
#include "list.h"

#ifdef INSTALLER

void setMigrationTextFile(std::wstring name, std::string data, bool overwrite = false);

// Task to migrate settings from (and optionally uninstall) any existing client.
class MigrateTask : public TaskList
{
public:
    using TaskList::TaskList;
    virtual void prepare() override;
    virtual double getEstimatedRollbackTime() const override { return 0.0; }
};

// Task to write remembered settings/account data.
class WriteSettingsTask : public Task
{
public:
    WriteSettingsTask(std::wstring settingsPath);
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 0.1; }
    virtual double getEstimatedRollbackTime() const override { return 0.0; }

private:
    std::wstring _settingsPath;
};

#endif // INSTALLER

#endif // TASKS_MIGRATE_H
