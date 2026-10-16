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
#include "../tasks.h"

class DcoDriverTask : public Task
{
protected:
    using Task::Task;
    static std::wstring getInfPath();
    static std::wstring getTapInstallPath();
    static bool _rollbackNeedsUninstall;
    static bool _rollbackNeedsReinstall;
    virtual double getEstimatedExecutionTime() const override
    {
        return 2.0;
    }
};

// When installing, UninstallDcoDriverTask does nothing, but performs the
// rollback for InstallDcoDriverTask.
class UninstallDcoDriverTask : public DcoDriverTask
{
public:
    using DcoDriverTask::DcoDriverTask;
    virtual void execute() override;
    virtual void rollback() override;
};

#ifdef INSTALLER

// Task to install Dco adapter.
class InstallDcoDriverTask : public DcoDriverTask
{
public:
    using DcoDriverTask::DcoDriverTask;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override
    {
        return 2.0;
    }
};

#endif // INSTALLER

#ifdef UNINSTALLER

// Helper task to perform final Dco driver cleanup. Does not fail.
class CleanupDcoDriverTask : public DcoDriverTask
{
public:
    using DcoDriverTask::DcoDriverTask;
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override
    {
        return 1.0;
    }
};

#endif // UNINSTALLER
