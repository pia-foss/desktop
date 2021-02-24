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

#ifndef TASKS_TAP_H
#define TASKS_TAP_H
#pragma once

#include "tasks.h"

// Base task for TAP installation/uninstallation.
class TapDriverTask : public Task
{
protected:
    using Task::Task;
    static std::wstring getInfPath();
    static bool _rollbackNeedsUninstall;
    static bool _rollbackNeedsReinstall;
};

// Task to uninstall TAP adapter. Forms a pair with InstallTapDriverTask.
// When installing, UninstallTapDriverTask does nothing, but performs the
// rollback for InstallTapDriverTask.
class UninstallTapDriverTask : public TapDriverTask
{
public:
    using TapDriverTask::TapDriverTask;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 2.0; }
};

#ifdef INSTALLER

// Task to install TAP adapter.
class InstallTapDriverTask : public TapDriverTask
{
public:
    using TapDriverTask::TapDriverTask;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 2.0; }
};

#endif // INSTALLER

#ifdef UNINSTALLER

// Helper task to perform final TAP driver cleanup. Does not fail.
class CleanupTapDriverTask : public TapDriverTask
{
public:
    using TapDriverTask::TapDriverTask;
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 1.0; }
};

#endif // UNINSTALLER

#endif // TASKS_TAP_H
