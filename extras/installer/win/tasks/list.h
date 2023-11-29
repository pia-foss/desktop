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

#ifndef TASKS_LIST_H
#define TASKS_LIST_H
#pragma once

#include "../tasks.h"
#include "function.h"

// Task that performs a list of subtasks.
class TaskList : public CaptionTask, public ProgressListener
{
public:
    using CaptionTask::CaptionTask;

    void add(std::shared_ptr<Task> task);
    template<class TaskType = FunctionTask, typename... Args> inline TaskType& addNew(Args&&... args);

    virtual void prepare() override;
    virtual void execute() override;
    virtual void rollback() override;
    virtual bool needsRollback() const override { return _currentTaskIndex >= 0; }
    virtual double getEstimatedExecutionTime() const override;
    virtual double getEstimatedRollbackTime() const override;
private:
    virtual void setProgress(double progress, double timeRemaining) override;
    virtual void setCaption(UIString caption) override;
private:
    std::vector<std::shared_ptr<Task>> _tasks;
    int _currentTaskIndex = -1;
    double _totalExecutionTime = 0.0; // Sum of all estimates
    double _completedExecutionTime = 0.0; // Sums of estimates (not actual time) of completed tasks
    double _currentExecutionTime = 0.0; // Estimated execution time for current task
    double _remainingRollbackTime = 0.0; // Estimate of remaining rollback tasks
    double _currentRollbackTime = 0.0; // Estimate of current rollback task
    bool _rollingBack = false;
};

template<class TaskType, typename... Args>
inline TaskType& TaskList::addNew(Args&&... args)
{
    auto task = std::make_shared<TaskType>(std::forward<Args>(args)...);
    add(task);
    return *task;
}

#endif // TASKS_LIST_H
