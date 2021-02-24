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

#ifndef TASKS_CALLOUT_H
#define TASKS_CALLOUT_H

#include "tasks.h"

class CalloutDriverTask : public Task
{
protected:
    using Task::Task;
    static std::wstring getInfPath();
    virtual double getEstimatedExecutionTime() const override { return 2.0; }
};

#ifdef UNINSTALLER

// Task to uninstall the callout driver.  Does not roll back - the daemon can
// offer to reinstall the callout driver, and typically a reboot is required to
// reinstall it anyway.
class UninstallCalloutDriverTask : public CalloutDriverTask
{
public:
    using CalloutDriverTask::CalloutDriverTask;
    virtual void execute() override;
    virtual void rollback() override;
};

#endif

#ifdef INSTALLER

// Task to update the callout driver.  Only updates it if it's already
// installed - doesn't install the driver if it isn't installed.
class UpdateCalloutDriverTask : public CalloutDriverTask
{
public:
    using CalloutDriverTask::CalloutDriverTask;
    virtual void execute() override;
    virtual void rollback() override;
};

#endif

#endif
