// Copyright (c) 2024 Private Internet Access, Inc.
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

#ifndef TASKS_REGISTRY_H
#define TASKS_REGISTRY_H
#pragma once

#include "../tasks.h"

// Note: these tasks need the SE_BACKUP_NAME and SE_RESTORE_NAME privileges.

// Split a registry key path including a root into a predefined root key and a
// path.
//
// For example, "HKLM\\Software\\Microsoft\\Windows" becomes
// - root: HKEY_LOCAL_MACHINE
// - path: "Software\\Microsoft\\Windows"
//
// The path returned points to the same data as the input string (just
// &rootedPath[5], etc., depending on root prefix length).
//
// The roots supported are listed in the implementation.
//
// If the path is malformed or the root is not recognized, this returns
// nullptrs.
std::pair<HKEY, utf16ptr> splitRegistryRootedPath(utf16ptr rootedPath);

class RegistryBackup
{
public:
    // Back up the registry key open in HKEY.  tracePath is just used for
    // tracing.
    void backup(HKEY hkey, utf16ptr tracePath);
    // Restore the backup taken by backup() to the root and path given.
    void restore(HKEY root, utf16ptr path);

private:
    std::wstring _existingKeyBackup;
};

// RegistryKeyTask is a basic registry task providing backup and rollback.
// Specific tasks should implement updateKey() to perform the actual
// installation or uninstallation; RegistryKeyTask calls this with the key
// handle.
class RegistryKeyTask : public Task
{
public:
    // Create RegistryKeyTask with a rooted path to be split with
    // splitRegistryRootedPath().
    RegistryKeyTask(std::wstring rootedPath);

public:
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override {return 0.1;}

private:
    // updateKey() should write the new values/subkeys/etc. to the registry key
    // opened by RegistryKeyTask.  The key is _not_ recreated if it already
    // existed, so there could be existing values.
    //
    // Return true if RegistryKeyTask should record an uninstall entry (most
    // tasks should do this - uninstall data is an exception because it has a
    // dedicated uninstall task).
    virtual bool updateKey(HKEY key) = 0;

private:
    std::wstring _rootedPath;
    std::pair<HKEY, utf16ptr> _splitPath;
    RegistryBackup _existingKeyBackup;
};

#ifdef INSTALLER

// Task to write Uninstall entry to registry.
class WriteUninstallRegistryTask : public RegistryKeyTask
{
public:
    WriteUninstallRegistryTask();
private:
    virtual bool updateKey(HKEY hkey) override;
};

// Task to write the piavpn URL handler to the registry.
class WriteUrlHandlerRegistryTask : public RegistryKeyTask
{
public:
    WriteUrlHandlerRegistryTask(utf16ptr clientPath);
private:
    virtual bool updateKey(HKEY hkey) override;
private:
    utf16ptr _clientPath;
};

#endif // INSTALLER

// Task to remove any registry key, with possible rollback.
class RemoveRegistryKeyTask : public Task
{
public:
    RemoveRegistryKeyTask(std::wstring rootedPath);

public:
    virtual void execute() override;
    virtual void rollback() override;
    virtual double getEstimatedExecutionTime() const override {return 0.1;}

private:
    std::wstring _rootedPath;
    std::pair<HKEY, utf16ptr> _splitPath;
    RegistryBackup _existingKeyBackup;
};

#ifdef UNINSTALLER

// Task to remove Uninstall entry from registry. Does not fail.
class RemoveUninstallRegistryTask : public Task
{
public:
    using Task::Task;
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 0.1; }
};

// Task to remove Run entries from registry. Does not fail.
class RemoveRunRegistryTask : public Task
{
public:
    using Task::Task;
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 0.1; }
};

#endif // UNINSTALLER

#endif // TASKS_REGISTRY_H
