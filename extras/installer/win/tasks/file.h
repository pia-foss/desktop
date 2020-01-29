// Copyright (c) 2020 Private Internet Access, Inc.
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

#ifndef TASKS_FILE_H
#define TASKS_FILE_H
#pragma once

#include "tasks.h"

enum FileCreateResult
{
    CreateFailed = 0,
    CreateSuccessful = 1,
    CreateNotNeeded = 2,
};

FileCreateResult createDirectory(utf16ptr path);
bool deleteDirectory(utf16ptr path);
bool deleteEntireDirectory(std::wstring path);
bool deleteRollbackDirectory();

std::wstring getShellFolder(int csidl);

// Create a temporary file in the backup directory. The file will be
// zero-sized and writable, so pass the MOVEFILE_REPLACE_EXISTING later.
std::wstring createBackupFile(bool nameOnly = false);
std::wstring createBackupDirectory(bool nameOnly = false);
std::wstring backupFileOrDirectory(utf16ptr path, bool keepDirectory);
bool restoreBackup(utf16ptr originalPath, utf16ptr backupPath);

// Task base class for tasks needing to create files, where any preexisting
// file or directory needs to be moved out of the way. It is up to subclasses
// to actually create/write the file.
class CreateFileTask : public Task
{
public:
    CreateFileTask(std::wstring path);
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 0.005; }
protected:
    std::wstring _path, _backup;
};

// Task to create a directory, where any preexisting file or directory needs
// to be moved out of the way.
class CreateDirectoryTask : public Task
{
public:
    CreateDirectoryTask(std::wstring path, bool skipBackup = false);
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 0.005; }
protected:
    std::wstring _path, _backup;
    bool _skipBackup;
    bool _created = false;
};

// Task to remove a file (equivalent to an empty CreateFileTask).
class RemoveFileTask : public CreateFileTask
{
public:
    using CreateFileTask::CreateFileTask;
};

// Task to remove a directory.
class RemoveDirectoryTask : public Task
{
public:
    RemoveDirectoryTask(std::wstring path, bool recursive);
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override { return 0.005; }
protected:
    std::wstring _path, _backup;
    bool _recursive;
    bool _rollbackNeedsCreate = false;
};

// Task to create the rollback directory. Place before any other file tasks.
class CreateRollbackDirectoryTask : public Task
{
public:
    virtual void execute() override;
    virtual void rollback() override;
};

#endif // TASKS_FILE_H
