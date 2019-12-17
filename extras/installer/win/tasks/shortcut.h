// Copyright (c) 2019 London Trust Media Incorporated
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

#ifndef TASKS_SHORTCUT_H
#define TASKS_SHORTCUT_H
#pragma once

#include "tasks.h"
#include "file.h"

bool createShortcut(utf16ptr shortcutPath, utf16ptr targetPath, utf16ptr arguments, utf16ptr comment);
bool deleteShortcut(utf16ptr shortcutPath);
bool createStartMenuShortcut(utf16ptr name, utf16ptr targetPath, utf16ptr arguments, utf16ptr comment);
bool deleteStartMenuShortcut(utf16ptr name);

// Task to add a shortcut to the start menu.
class AddShortcutTask : public CreateFileTask
{
public:
    AddShortcutTask(utf16ptr name, std::wstring targetPath, std::wstring arguments = {}, std::wstring comment = {});
    virtual void execute() override;
    virtual double getEstimatedExecutionTime() const override { return 0.1; }
private:
    std::wstring _name, _targetPath, _arguments, _comment;
};

// Task to remove a shortcut from the start menu.
class RemoveShortcutTask : public RemoveFileTask
{
public:
    RemoveShortcutTask(utf16ptr name);
};

#endif // TASKS_SHORTCUT_H
