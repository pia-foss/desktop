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

#include "uninstall.h"
#include "shortcut.h"
#include "registry.h"

std::wstring getUninstallDataPath(const std::wstring& uninstallPath)
{
    return uninstallPath + L"\\" UNINSTALL_DATA_FILENAME;
}

#ifdef INSTALLER

typedef std::pair<std::string, std::wstring> UninstallEntry;

static std::vector<UninstallEntry> g_uninstallData;

void recordUninstallAction(std::string type, std::wstring path)
{
    g_uninstallData.push_back(UninstallEntry { std::move(type), std::move(path) });
}

#endif // INSTALLER

ExecuteUninstallDataTask::ExecuteUninstallDataTask(std::wstring uninstallPath)
    : _uninstallPath(std::move(uninstallPath))
{

}

#include <stack>

void ExecuteUninstallDataTask::prepare()
{
    std::wstring uninstallDataPath = _uninstallPath + L"\\" UNINSTALL_DATA_FILENAME;
    if (PathFileExists(uninstallDataPath.c_str()))
    {
        std::vector<std::string> lines;
        {
            std::stringstream buffer;
            std::ifstream file(uninstallDataPath);
            buffer << file.rdbuf();
            file.close();
            std::string line;
            while (std::getline(buffer, line))
            {
                if (!line.empty()) lines.push_back(std::move(line));
            }
        }
        // Iterate over the lines in reverse order
        while (!lines.empty())
        {
            auto entry = std::move(lines.back());
            lines.pop_back();;
            auto split = entry.find(' ');
            if (split == entry.npos || split == entry.size() - 1) continue;

            const auto &entryPath = utf16(&entry[split + 1]);
            entry.resize(split);

            if (entry == "FILE")
                addNew<RemoveFileTask>(getInstalledFilePath(entryPath));
            else if (entry == "DIR")
                addNew<RemoveDirectoryTask>(getInstalledFilePath(entryPath), false);
            else if (entry == "SHORTCUT")
                addNew<RemoveShortcutTask>(std::move(entryPath));
            // Remove an entire registry key (including values and subkeys)
            // The name permits a possible "REGISTRYVALUE" type if needed in the
            // future
            else if (entry == "REGISTRYKEY")
                addNew<RemoveRegistryKeyTask>(std::move(entryPath));
            else
                LOG("Unrecognized uninstall entry type %s", entry);
        }
    }

    TaskList::prepare();
}

void ExecuteUninstallDataTask::execute()
{
    LOG("Removing existing files");
    TaskList::execute();
}

std::wstring ExecuteUninstallDataTask::getInstalledFilePath(const std::wstring &entryPath)
{
    std::wstring path = _uninstallPath;
    if (path.back() != '\\')
        path.push_back('\\');
    path.append(entryPath);
    return path;
}

#ifdef INSTALLER

WriteUninstallDataTask::WriteUninstallDataTask(const std::wstring& installPath)
    : CreateFileTask(getUninstallDataPath(installPath))
{

}

void WriteUninstallDataTask::execute()
{
    recordUninstallAction("FILE", L"" UNINSTALL_DATA_FILENAME);

    CreateFileTask::execute();
    std::ofstream file;
    file.open(_path, std::ios::out);
    for (const auto& p : g_uninstallData)
    {
        file << p.first << ' ' << UTF8(p.second) << std::endl;
    }
    file.close();
}

#endif // INSTALLER
