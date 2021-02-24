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

#include "file.h"
#include <shlobj_core.h>
#pragma comment(lib, "shell32.lib")

static std::wstring g_rollbackPath;

// Create a directory (deletes any file that may be in the way)
FileCreateResult createDirectory(utf16ptr path)
{
    if (CreateDirectory(path, NULL))
        return CreateSuccessful;
    switch (GetLastError())
    {
    case ERROR_ALREADY_EXISTS:
        if (PathIsDirectory(path))
            return CreateNotNeeded;
        if (!DeleteFile(path))
        {
            LOG("Unable to delete file in the way of directory %ls (%d)", path, GetLastError());
            return CreateFailed;
        }
        if (CreateDirectory(path, NULL))
            return CreateSuccessful;
        // fallthrough
    default:
        LOG("Unable to create directory %ls (%d)", path, GetLastError());
        return CreateFailed;
    }
}

bool deleteDirectory(utf16ptr path)
{
    return !!RemoveDirectory(path);
}

bool deleteEntireDirectory(std::wstring path)
{
    // Need to double-terminate this string
    path.push_back(0);
    //
    SHFILEOPSTRUCT removeDirectory = {
        NULL,
        FO_DELETE,
        path.c_str(),
        NULL,
        FOF_NO_UI,
        false,
        0,
        NULL
    };
    int result = SHFileOperation(&removeDirectory);
    if (result != 0)
    {
        LOG("SHFileOperation returned %d", result);
        return false;
    }
    return true;
}

bool deleteRollbackDirectory()
{
    if (!g_rollbackPath.empty())
        return deleteEntireDirectory(g_rollbackPath);
    return true;
}

std::wstring getShellFolder(int csidl)
{
    std::wstring result;
    result.resize(MAX_PATH, 0);
    if (S_OK != SHGetFolderPathW(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, &result[0]))
        InstallerError::abort(UIString(IDS_MB_FAILEDTOGETSHELLFOLDER, csidl));
    result.resize(wcslen(&result[0]));
    return result;
}

std::wstring createBackupFile(bool nameOnly)
{
    std::wstring buf(MAX_PATH, 0);
    if (!GetTempFileName(g_rollbackPath.c_str(), _T("pia"), 0, &buf[0]))
    {
        LOG("Unable to create temporary file");
        return std::wstring();
    }
    buf.resize(wcslen(buf.c_str()));
    if (nameOnly)
    {
        if (!DeleteFile(buf.c_str()))
            LOG("Unable to delete temporary file");
    }
    return buf;
}

std::wstring createBackupDirectory(bool nameOnly)
{
    std::wstring path = createBackupFile(true);
    if (!path.empty() && !nameOnly && !CreateDirectory(path.c_str(), NULL))
    {
        LOG("Unable to create temporary directory");
        return std::wstring();
    }
    return path;
}

std::wstring backupFileOrDirectory(utf16ptr path, bool keepDirectory)
{
    std::wstring backupPath;
    if (!g_rollbackPath.empty() && PathFileExists(g_rollbackPath.c_str()) && PathFileExists(path))
    {
        LOG("Backing up %ls", path);
        if (PathIsDirectory(path))
        {
            if (keepDirectory)
            {
                // Nothing needs to be done
                return std::wstring();
            }
            else
            {
                backupPath = createBackupDirectory(true);
            }
        }
        else
        {
            backupPath = createBackupFile(true);
        }
        if (!backupPath.empty())
        {
            while(!MoveFileEx(path, backupPath.c_str(), MOVEFILE_WRITE_THROUGH | MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
            {
                LOG("Unable to move %ls (%d)", path, GetLastError());
                if(Ignore == InstallerError::raise(Abort | Retry | Ignore, UIString{IDS_MB_UNABLETOMOVEPATH, path}))
                {
                    // Proceed without backing up the file
                    return std::wstring();
                }
            }
        }
    }
    return backupPath;
}

bool restoreBackup(utf16ptr originalPath, utf16ptr backupPath)
{
    return !!MoveFileEx(backupPath, originalPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
}

CreateFileTask::CreateFileTask(std::wstring path)
    : _path(std::move(path))
{

}

void CreateFileTask::execute()
{
    _backup = backupFileOrDirectory(_path, false);
}

void CreateFileTask::rollback()
{
    if (!_backup.empty())
        LOG("Rollback restore %ls", _path);
    else
        LOG("Rollback delete %ls", _path);
    if (!DeleteFileW(_path.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
        LOG("Rollback delete %ls failed (%d)", _path, GetLastError());
    if (!_backup.empty() && !restoreBackup(_path, _backup))
        LOG("Rollback restore of %ls failed (%d)", _path, GetLastError());
}

CreateDirectoryTask::CreateDirectoryTask(std::wstring path, bool skipBackup)
    : _path(std::move(path)), _skipBackup(skipBackup)
{

}

void CreateDirectoryTask::execute()
{
    if (!_skipBackup)
        _backup = backupFileOrDirectory(_path, true);
    if (CreateSuccessful == retryLoop(createDirectory(_path), UIString(IDS_MB_UNABLETOCREATEDIRECTORY, _path)))
        _created = true;
}

void CreateDirectoryTask::rollback()
{
    if (!_backup.empty())
        LOG("Rollback restore %ls", _path);
    else
        LOG("Rollback delete %ls", _path);
    if (_created && !RemoveDirectory(_path.c_str()))
        LOG("Rollback delete %ls failed (%d)", _path, GetLastError());
    if (!_backup.empty() && !restoreBackup(_path, _backup))
        LOG("Rollback restore %ls failed (%d)", _path, GetLastError());
}

RemoveDirectoryTask::RemoveDirectoryTask(std::wstring path, bool recursive)
    : _path(std::move(path)), _recursive(recursive)
{

}

void RemoveDirectoryTask::execute()
{
    do
    {
        if (RemoveDirectory(_path.c_str()))
        {
            _rollbackNeedsCreate = true;
            return;
        }
        if (GetLastError() == ERROR_NOT_FOUND)
            return;
        if (_recursive && GetLastError() == ERROR_DIR_NOT_EMPTY)
        {
            _backup = backupFileOrDirectory(_path, false);
            if (!_backup.empty())
                return;
        }
        else
            LOG("Error removing directory %ls (%d)", _path, GetLastError());
    } while (Retry == InstallerError::raise(Abort | Retry | Ignore, UIString{IDS_MB_UNABLETOREMOVEDIR, _path}));
}

void RemoveDirectoryTask::rollback()
{
    if (!_backup.empty())
        restoreBackup(_path, _backup);
    else if (_rollbackNeedsCreate)
        CreateDirectory(_path.c_str(), NULL);
}

void CreateRollbackDirectoryTask::execute()
{
    g_rollbackPath = g_installPath + L"\\install.tmp";
    if (retryIgnoreLoop(createDirectory(g_rollbackPath), IDS_MB_UNABLETOCREATEROLLBACKDIR))
    {
        SetFileAttributes(g_rollbackPath.c_str(), FILE_ATTRIBUTE_HIDDEN);
    }
    else
    {
        LOG("Proceeding without rollback directory");
        g_rollbackPath.clear();
    }
}

void CreateRollbackDirectoryTask::rollback()
{
    if (!g_rollbackPath.empty())
    {
        LOG("Deleting rollback directory");
        deleteEntireDirectory(g_rollbackPath.c_str());
    }
}
