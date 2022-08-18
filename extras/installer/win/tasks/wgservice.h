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

#ifndef TASKS_WGSERVICE_H
#define TASKS_WGSERVICE_H
#pragma once

#include "../tasks.h"

#ifdef UNINSTALLER
class UninstallWgServiceTask : public Task {
protected:
    using Task::Task;
    virtual void execute () override;
    virtual void rollback () override;
    virtual double getEstimatedExecutionTime() const override { return 0.5; }
};
#endif

#ifdef INSTALLER
class InstallWgServiceTask : public Task {
protected:
    using Task::Task;
    virtual void execute () override;
    virtual void rollback () override;
    virtual double getEstimatedExecutionTime() const override { return 0.5; }
};
#endif // INSTALLER

#endif // TASKS_WGSERVICE_H
