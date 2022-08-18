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

#ifndef TASKS_FUNCTION_H
#define TASKS_FUNCTION_H
#pragma once

#include "../tasks.h"

// Task whose implementation is delegated to separate functions.
class FunctionTask : public Task
{
public:
    FunctionTask(std::function<void()> execute, double estimatedExecutionTime = 0.0);
    FunctionTask(std::function<void()> execute, std::function<void()> rollback, double estimatedRollbackTime = 0.0);
    FunctionTask(std::function<void()> execute, double estimatedExecutionTime, std::function<void()> rollback, double estimatedRollbackTime = 0.0);
    FunctionTask(UIString caption, std::function<void()> execute, double estimatedExecutionTime = 0.0);
    FunctionTask(UIString caption, std::function<void()> execute, std::function<void()> rollback, double estimatedRollbackTime = 0.0);
    FunctionTask(UIString caption, std::function<void()> execute, double estimatedExecutionTime, std::function<void()> rollback, double estimatedRollbackTime = 0.0);

    // Attach the function to execute for this task (along with its estimated execution time)
    FunctionTask& execute(std::function<void()> execute, double estimatedTime = 0.0);
    // Attach the function for rolling back this task (along with its estimated execution time)
    FunctionTask& rollback(std::function<void()> rollback, double estimatedTime = 0.0);
    // Attach a caption to display while performing this task
    FunctionTask& caption(UIString caption);

    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override;
    virtual double getEstimatedRollbackTime() const override;
private:
    std::function<void()> _execute, _rollback;
    UIString _caption;
    double _estimatedExecutionTime = 0.0, _estimatedRollbackTime = 0.0;
};

#endif // TASKS_FUNCTION_H
