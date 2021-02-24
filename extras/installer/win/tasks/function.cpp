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

#include "function.h"
FunctionTask::FunctionTask(std::function<void()> execute, double estimatedExecutionTime)
{
    this->execute(std::move(execute), estimatedExecutionTime);
}

FunctionTask::FunctionTask(std::function<void()> execute, std::function<void()> rollback, double estimatedRollbackTime)
{
    this->execute(std::move(execute));
    this->rollback(std::move(rollback), estimatedRollbackTime);
}

FunctionTask::FunctionTask(std::function<void()> execute, double estimatedExecutionTime, std::function<void()> rollback, double estimatedRollbackTime)
{
    this->execute(std::move(execute), estimatedExecutionTime);
    this->rollback(std::move(rollback), estimatedRollbackTime);
}

FunctionTask::FunctionTask(UIString caption, std::function<void()> execute, double estimatedExecutionTime)
{
    this->caption(std::move(caption));
    this->execute(std::move(execute), estimatedExecutionTime);
}

FunctionTask::FunctionTask(UIString caption, std::function<void()> execute, std::function<void()> rollback, double estimatedRollbackTime)
{
    this->caption(std::move(caption));
    this->execute(std::move(execute));
    this->rollback(std::move(rollback), estimatedRollbackTime);
}

FunctionTask::FunctionTask(UIString caption, std::function<void()> execute, double estimatedExecutionTime, std::function<void()> rollback, double estimatedRollbackTime)
{
    this->caption(std::move(caption));
    this->execute(std::move(execute), estimatedExecutionTime);
    this->rollback(std::move(rollback), estimatedRollbackTime);
}

FunctionTask& FunctionTask::execute(std::function<void()> execute, double estimatedTime)
{
    _execute = std::move(execute);
    _estimatedExecutionTime = estimatedTime;
    return *this;
}

FunctionTask& FunctionTask::rollback(std::function<void()> rollback, double estimatedTime)
{
    _rollback = std::move(rollback);
    _estimatedRollbackTime = estimatedTime;
    return *this;
}

FunctionTask& FunctionTask::caption(UIString caption)
{
    _caption = std::move(caption);
    return *this;
}

void FunctionTask::execute()
{
    if (_caption) _listener->setCaption(_caption);
    if (_execute) _execute();
}

void FunctionTask::rollback()
{
    if (_rollback) _rollback();
}

double FunctionTask::getEstimatedExecutionTime() const
{
    return _estimatedExecutionTime;
}

double FunctionTask::getEstimatedRollbackTime() const
{
    return _estimatedRollbackTime;
}
