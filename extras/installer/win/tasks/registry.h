// Copyright (c) 2019 London Trust Media Incorporated
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

#ifndef TASKS_REGISTRY_H
#define TASKS_REGISTRY_H
#pragma once

#include "tasks.h"

extern const wchar_t g_uninstallRegistryKey[];

// Note: these tasks need the SE_BACKUP_NAME and SE_RESTORE_NAME privileges.

std::vector<std::wstring> loadAllUserRegistryHives();

#ifdef INSTALLER

// Task to write Uninstall entry to registry.
class WriteUninstallRegistryTask : public Task
{
public:
    using Task::Task;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 0.1; }
private:
    std::wstring _existingKeyBackup;
};

#endif // INSTALLER

#ifdef UNINSTALLER

// Task to remove Uninstall entry from registry. Does not fail.
class RemoveUninstallRegistryTask : public Task
{
public:
    using Task::Task;
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 0.1; }
};

// Task to remove Run entries from registry. Does not fail.
class RemoveRunRegistryTask : public Task
{
public:
    using Task::Task;
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 0.1; }
};

#endif // UNINSTALLER

#endif // TASKS_REGISTRY_H
