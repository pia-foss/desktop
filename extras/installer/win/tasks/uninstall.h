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

#ifndef TASKS_UNINSTALL_H
#define TASKS_UNINSTALL_H
#pragma once

#include "../tasks.h"
#include "file.h"
#include "list.h"

#define UNINSTALL_DATA_FILENAME "uninstall.dat"

std::wstring getUninstallDataPath(const std::wstring& uninstallPath);

// Task to remove an existing installation by obeying its uninstall.dat file.
class ExecuteUninstallDataTask : public TaskList
{
public:
    ExecuteUninstallDataTask(std::wstring uninstallPath);
    virtual void prepare() override;
    virtual void execute() override;
private:
    std::wstring getInstalledFilePath(const std::wstring& relativePath);
private:
    std::wstring _uninstallPath;
};

#ifdef INSTALLER

// Task to write an uninstall.dat file (data collected via a global).
class WriteUninstallDataTask : public CreateFileTask
{
public:
    WriteUninstallDataTask(const std::wstring& installPath);
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 0.1; }
};

#endif // INSTALLER

#endif // TASKS_UNINSTALL_H
