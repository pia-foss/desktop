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

#include "list.h"
void TaskList::add(std::shared_ptr<Task> task)
{
    task->setListener(this);
    _tasks.push_back(std::move(task));
}

void TaskList::prepare()
{
    _totalExecutionTime = 0.0;
    for (const auto& task : _tasks)
    {
        task->prepare();
        _totalExecutionTime += task->getEstimatedExecutionTime();
    }
    _completedExecutionTime = 0.0;
    _remainingRollbackTime = 0.0;
}

void TaskList::execute()
{
    CaptionTask::execute();
    for (_currentTaskIndex = 0; _currentTaskIndex < _tasks.size(); _currentTaskIndex++)
    {
        _currentExecutionTime = _tasks[_currentTaskIndex]->getEstimatedExecutionTime();

        setProgress(0.0, _currentExecutionTime);
        _tasks[_currentTaskIndex]->execute();
        setProgress(1.0, 0.0);

        checkAbort();

        // Add time estimates only last in the loop, so there are no interruptions
        // between this and the next iteration. This preserves the invariant that
        // the times for the current task index have not been added to totals.
        _completedExecutionTime += _currentExecutionTime;
        _remainingRollbackTime += _tasks[_currentTaskIndex]->getEstimatedRollbackTime();
    }
}

void TaskList::rollback()
{
    _rollingBack = true;
    // If we never started processing the list, there's nothing to do.
    if (_currentTaskIndex < 0)
        return;
    if (_currentTaskIndex < _tasks.size())
    {
        // If we were aborted mid-list, the last time estimates haven't been added yet.
        _completedExecutionTime += _tasks[_currentTaskIndex]->getEstimatedExecutionTime();
        _remainingRollbackTime += _tasks[_currentTaskIndex]->getEstimatedRollbackTime();
    }
    else
    {
        // Otherwise we were done iterating, so begin rolling back at the last task.
        _currentTaskIndex = _tasks.size() - 1;
    }
    for ( ; _currentTaskIndex >= 0; _currentTaskIndex--)
    {
        // Pop the time estimates for the task being currently rolled back.
        _currentExecutionTime = _tasks[_currentTaskIndex]->getEstimatedExecutionTime();
        _currentRollbackTime = _tasks[_currentTaskIndex]->getEstimatedRollbackTime();

        _completedExecutionTime -= _currentExecutionTime;
        _remainingRollbackTime -= _currentRollbackTime;

        _tasks[_currentTaskIndex]->rollback();
        setProgress(0.0, 0.0);
    }
}

double TaskList::getEstimatedExecutionTime() const
{
    return _totalExecutionTime;
}

double TaskList::getEstimatedRollbackTime() const
{
    double rollbackTime = _remainingRollbackTime;
    // If we're mid-list, add the current task's estimated rollback time as well.
    if (_currentTaskIndex >= 0 && _currentTaskIndex < _tasks.size())
        rollbackTime += _tasks[_currentTaskIndex]->getEstimatedRollbackTime();
    return rollbackTime;
}

static double clampProgress(double progress)
{
    if (progress >= 1.0)
        return 1.0;
    else if (progress > 0.0)
        return progress;
    else
        return 0.0;
}

static double clampTimeRemaining(double timeRemaining)
{
    if (timeRemaining > 0.0)
        return timeRemaining;
    else
        return 0.0;
}

void TaskList::setProgress(double progress, double timeRemaining)
{
    double totalProgress = _completedExecutionTime + (progress * _currentExecutionTime);
    double outerProgress = _totalExecutionTime > 0.0 ? totalProgress / _totalExecutionTime : 0.0;
    double outerTimeRemaining;
    if (_rollingBack)
        outerTimeRemaining = _remainingRollbackTime + timeRemaining;
    else
        outerTimeRemaining = _totalExecutionTime - _completedExecutionTime - _currentExecutionTime + timeRemaining;
    _listener->setProgress(clampProgress(outerProgress), clampTimeRemaining(outerTimeRemaining));
}

void TaskList::setCaption(UIString caption)
{
    _listener->setCaption(std::move(caption));
}
