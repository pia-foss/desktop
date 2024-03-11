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

#include "shortcut.h"
#include <shlobj_core.h>
#include <shobjidl.h>

#pragma comment(lib, "ole32.lib")

bool createShortcut(utf16ptr shortcutPath, utf16ptr targetPath, utf16ptr arguments, utf16ptr comment)
{
    HRESULT result;
    IShellLink* shellLink;
    if (SUCCEEDED(result = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&shellLink)))
    {
        shellLink->SetPath(targetPath);
        if (!arguments.empty())
            shellLink->SetArguments(arguments);
        if (!comment.empty())
            shellLink->SetDescription(comment);

        IPersistFile* persistFile;
        if (SUCCEEDED(result = shellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&persistFile)))
        {
            result = persistFile->Save(shortcutPath, TRUE);
            persistFile->Release();
        }
        shellLink->Release();
    }
    return SUCCEEDED(result);
}

bool deleteShortcut(utf16ptr shortcutPath)
{
    return !!DeleteFile(shortcutPath);
}

static std::wstring getShortcutPath(utf16ptr name)
{
    return g_startMenuPath + L"\\" + name.ptr + L".lnk";
}

bool createStartMenuShortcut(utf16ptr name, utf16ptr targetPath, utf16ptr arguments, utf16ptr comment)
{
    return createShortcut(getShortcutPath(name), targetPath, arguments, comment);
}

bool deleteStartMenuShortcut(utf16ptr name)
{
    return deleteShortcut(getShortcutPath(name));
}

AddShortcutTask::AddShortcutTask(utf16ptr name, std::wstring targetPath, std::wstring arguments, std::wstring comment)
    : CreateFileTask(getShortcutPath(name))
    , _name(std::move(name))
    , _targetPath(std::move(targetPath))
    , _arguments(std::move(arguments))
    , _comment(std::move(comment))
{

}

void AddShortcutTask::execute()
{
    recordUninstallAction("SHORTCUT", _name);

    CreateFileTask::execute();
    if (!createShortcut(_path, _targetPath, _arguments, _comment))
        LOG("Unable to create shortcut");
}

RemoveShortcutTask::RemoveShortcutTask(utf16ptr name)
    : RemoveFileTask(getShortcutPath(name))
{

}
