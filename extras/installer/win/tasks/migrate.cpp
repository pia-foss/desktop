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

#include "migrate.h"
#include "function.h"
#include "process.h"
#include "uninstall.h"
#include "../util.h"
#include "brand.h"
#include <shlobj.h>

#include <map>

#ifdef INSTALLER

static std::map<std::wstring, std::string> g_migrationFiles;

void setMigrationTextFile(std::wstring name, std::string data, bool overwrite)
{
    if (data.empty()) return;
    auto it = g_migrationFiles.find(name);
    if (it == g_migrationFiles.end())
        g_migrationFiles.insert(std::make_pair(std::move(name), std::move(data)));
    else if (overwrite)
        it->second = std::move(data);
}

void MigrateTask::prepare()
{
    // Migrate settings from old daemon data path
    setMigrationTextFile(L"settings.json", readTextFile(g_oldDaemonDataPath + L"\\settings.json"));
    setMigrationTextFile(L"account.json", readTextFile(g_oldDaemonDataPath + L"\\account.json"));
    setMigrationTextFile(L"data.json", readTextFile(g_oldDaemonDataPath + L"\\data.json"));
    setMigrationTextFile(L"debug.txt", readTextFile(g_oldDaemonDataPath + L"\\debug.txt"));

    const char* drive = getenv("SystemDrive");
    if (!drive) drive = "C:";
    auto oldDesktopPath = wstrprintf(L"%s\\Program Files\\pia_manager", utf16(drive));
    if (PathFileExistsW(oldDesktopPath.c_str()))
    {
        if (IDYES != messageBox(IDS_MB_REPLACEINSTALLATION, IDS_MB_CAP_REPLACEINSTALLATION, 0, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, IDYES))
            throw InstallerAbort();

        // Save a copy of the old settings if they exist
        std::string oldSettings = readTextFile(oldDesktopPath + L"\\data\\settings.json");
        if (!oldSettings.empty())
            setMigrationTextFile(L"settings.json", strprintf("{\"legacy\":%s}", oldSettings));

        addNew<FunctionTask>(IDS_CAPTION_REMOVINGPREVIOUSVERSION, [=] {
            while (isProcessRunning(L"pia_manager.exe"))
            {
                InstallerError::raise(Abort | Retry, IDS_MB_PLEASEEXIT);
                Sleep(1000);
            }
            auto oldDesktopUninstaller = oldDesktopPath + L"\\unins000.exe";
            if (PathFileExistsW(oldDesktopUninstaller.c_str()))
            {
                for (;;)
                {
                    if (0 == runProgram(oldDesktopUninstaller, { L"/SILENT" }))
                        break;
                    Sleep(500);
                    if (!PathFileExistsW(oldDesktopPath.c_str()))
                        break;
                    if (Ignore == InstallerError::raise(PathFileExistsW(oldDesktopUninstaller.c_str()) ? Abort | Retry | Ignore : Abort | Ignore, IDS_MB_PROBLEMUNINSTALLING))
                        break;
                }
            }
            if (PathFileExistsW(oldDesktopPath.c_str()))
            {
                deleteEntireDirectory(oldDesktopPath);
            }
        }, 10.0);
    }
}

WriteSettingsTask::WriteSettingsTask(std::wstring settingsPath)
    : _settingsPath(std::move(settingsPath))
{

}

void WriteSettingsTask::execute()
{
    for (const auto& p : g_migrationFiles)
    {
        auto path = _settingsPath + L"\\" + p.first;
        // Write the file only if the target doesn't already exist
        writeTextFile(path, p.second, CREATE_NEW);
    }

    // If the user has a .pia-early-debug file, create data\.pia-early-debug, so
    // the daemon will enable logging on the first startup.
    std::wstring earlyDebugFile{L"\\." BRAND_CODE L"-early-debug"};
    auto userEarlyDebug = getShellFolder(CSIDL_PROFILE) + earlyDebugFile;
    if(::PathFileExistsW(userEarlyDebug.c_str()))
    {
        writeTextFile(_settingsPath + earlyDebugFile, "", CREATE_ALWAYS);
    }
}

#endif // INSTALLER
