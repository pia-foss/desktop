// Copyright (c) 2023 Private Internet Access, Inc.
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

#ifndef TASKS_SERVICE_H
#define TASKS_SERVICE_H
#pragma once

#include "../tasks.h"

// Base task for service management.
class ServiceTask : public Task
{
protected:
    using Task::Task;
    static bool _rollbackNeedsReinstall;

public:
    static void reinstallOnRollback();
};

// Task to stop the service.
class StopExistingServiceTask : public ServiceTask
{
public:
    StopExistingServiceTask(LPCWSTR pSvcName, bool restartOnRollback);

    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 0.5; }
private:
    LPCWSTR _pSvcName;
    bool _restartOnRollback;
    bool _rollbackNeedsStart;
};

// Task to uninstall the service. Forms a pair with InstallServiceTask.
// When installing, UninstallExistingServiceTask does nothing, but performs
// the rollback for InstallServiceTask.
class UninstallExistingServiceTask : public ServiceTask
{
public:
    using ServiceTask::ServiceTask;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 0.5; }
};

#ifdef INSTALLER

// Task to install the service.
class InstallServiceTask : public ServiceTask
{
public:
    using ServiceTask::ServiceTask;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 0.25; }
};

// Task to start the service.
class StartInstalledServiceTask : public ServiceTask
{
public:
    using ServiceTask::ServiceTask;
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 0.5; }
};

#endif // INSTALLER

#endif // TASKS_SERVICE_H
