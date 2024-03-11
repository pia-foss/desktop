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

#include "registry.h"
#include "file.h"
#include "brand.h"
#include "product.h"
#include "version_literal.h"
#include <cassert>

namespace
{
    const wchar_t g_uninstallRegistryKey[] = L"HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" BRAND_WINDOWS_PRODUCT_GUID;
    const wchar_t g_urlHandlerRegistryKey[] = L"HKCR\\" BRAND_CODE "vpn";

    // Check if a UTF-16 value starts with a known prefix.  If it does, returns
    // the position following the prefix (the rest of the string).  Otherwise,
    // returns nullptr.
    //
    // If the two strings are exactly the same, returns an empty string (it
    // matches, but there is no content left).  (Specifically, this returns the
    // pointer to the original value's null terminator.)
    template<std::size_t len>
    utf16ptr matchPrefix(const wchar_t (&prefix)[len], utf16ptr value)
    {
        assert(prefix[len-1] == 0); // Null-terminated
        if(std::wcsncmp(prefix, value, len-1) == 0)
            return &value[len-1];
        return nullptr;
    }

    HKEY openSubkey(HKEY root, utf16ptr path)
    {
        HKEY hkey{nullptr};
        DWORD disposition;
        if(LSTATUS err = RegCreateKeyExW(root, path, 0, NULL, 0, KEY_ALL_ACCESS,
                                         NULL, &hkey, &disposition))
        {
            LOG("Failed to create/open registry key %ls (%d)",
                path, err);
            return nullptr;
        }

        utf16ptr dispositionStr{L"Unknown"};
        if(disposition == REG_OPENED_EXISTING_KEY)
            dispositionStr = L"Opened existing";
        else if(disposition == REG_CREATED_NEW_KEY)
            dispositionStr = L"Created";
        LOG("Opened subkey %ls - (%d) %ls", path, disposition, dispositionStr);
        return hkey;
    }
}

std::pair<HKEY, utf16ptr> splitRegistryRootedPath(utf16ptr rootedPath)
{
    utf16ptr path{nullptr};
    path = matchPrefix(L"HKCR\\", rootedPath);
    if(path)
        return {HKEY_CLASSES_ROOT, path};
    path = matchPrefix(L"HKCC\\", rootedPath);
    if(path)
        return {HKEY_CURRENT_CONFIG, path};
    path = matchPrefix(L"HKCU\\", rootedPath);
    if(path)
        return {HKEY_CURRENT_USER, path};
    path = matchPrefix(L"HKLM\\", rootedPath);
    if(path)
        return {HKEY_LOCAL_MACHINE, path};
    path = matchPrefix(L"HKU\\", rootedPath);
    if(path)
        return {HKEY_USERS, path};
    LOG("Unrecognized registry path: %ls", rootedPath);
    return {nullptr, nullptr};
}

void RegistryBackup::backup(HKEY hkey, utf16ptr tracePath)
{
    _existingKeyBackup = createBackupFile();
    if (!_existingKeyBackup.empty())
    {
        DeleteFileW(_existingKeyBackup.c_str());
        if(LSTATUS err = RegSaveKeyExW(hkey, _existingKeyBackup.c_str(), NULL, REG_LATEST_FORMAT))
        {
            LOG("Unable to backup registry key %ls (%d)", tracePath, err);
        }
        if (!PathFileExistsW(_existingKeyBackup.c_str()))
            _existingKeyBackup.clear();
    }
}

void RegistryBackup::restore(HKEY root, utf16ptr path)
{
    if (!_existingKeyBackup.empty())
    {
        LOG("Restoring registry key %ls from backup", path);
        HKEY hkey;
        if (LSTATUS err = RegCreateKeyExW(root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL))
        {
            LOG("Failed to create registry key %ls (%d)", path, err);
            return;
        }

        if (LSTATUS err = RegRestoreKeyW(hkey, _existingKeyBackup.c_str(), REG_FORCE_RESTORE))
            LOG("Failed to restore registry key %ls (%d)", path, err);
        RegCloseKey(hkey);
    }
}

RegistryKeyTask::RegistryKeyTask(std::wstring rootedPath)
    : _rootedPath{std::move(rootedPath)}, _splitPath{nullptr, nullptr}
{
    _splitPath = splitRegistryRootedPath(_rootedPath);
}

void RegistryKeyTask::execute()
{
    // The registry keys set by this installer currently aren't critical, if for
    // some reason the path was malformed, skip it.  Should not normally happen
    // though.
    if(!_splitPath.first || !_splitPath.second)
    {
        LOG("Can't install registry key %ls, path not valid", _rootedPath.c_str());
        return;
    }

    HKEY hkey;
    DWORD disposition;
    if(LSTATUS err = RegCreateKeyExW(_splitPath.first, _splitPath.second, 0,
                                     NULL, 0, KEY_ALL_ACCESS, NULL, &hkey,
                                     &disposition))
    {
        LOG("Failed to create/open registry key %ls (%d)",
            _rootedPath.c_str(), err);
        return;
    }

    // If we opened an existing key, back it up
    if (disposition == REG_OPENED_EXISTING_KEY)
        _existingKeyBackup.backup(hkey, _rootedPath.c_str());
    // Otherwise, we don't have to do anything but delete the key

    if(updateKey(hkey))
        recordUninstallAction("REGISTRYKEY", _rootedPath);

    RegCloseKey(hkey);
}

void RegistryKeyTask::rollback()
{
    if(LSTATUS err = RegDeleteTreeW(_splitPath.first, _splitPath.second))
    {
        LOG("Failed to delete registry key %ls (%d)", _rootedPath.c_str(), err);
    }
    _existingKeyBackup.restore(_splitPath.first, _splitPath.second);
}

#ifdef INSTALLER

static size_t g_installationSize = 0;

void recordInstallationSize(size_t size)
{
    g_installationSize = size;
}

static void writeRegistry(HKEY hkey, utf16ptr name, DWORD type, const void* data, size_t size)
{
    if (LSTATUS err = RegSetValueExW(hkey, name, 0, type, (LPCBYTE)data, size))
        LOG("Unable to write registry value %ls (%d)", name, err);
}
static void writeRegistry(HKEY hkey, utf16ptr name, DWORD type, utf16ptr value)
{
    writeRegistry(hkey, name, type, value.ptr, value.ptr ? (value.length() + 1) * 2 : 0);
}
static void writeRegistry(HKEY hkey, utf16ptr name, DWORD value)
{
    writeRegistry(hkey, name, REG_DWORD, &value, sizeof(DWORD));
}

WriteUninstallRegistryTask::WriteUninstallRegistryTask()
    : RegistryKeyTask{g_uninstallRegistryKey}
{
}

bool WriteUninstallRegistryTask::updateKey(HKEY hkey)
{
    LOG("Writing uninstall entry");

    // Translation note - product and company names are not translated.
    writeRegistry(hkey, L"DisplayName", REG_SZ, L"" PIA_PRODUCT_NAME);
    writeRegistry(hkey, L"InstallLocation", REG_EXPAND_SZ, g_installPath);
    writeRegistry(hkey, L"UninstallString", REG_EXPAND_SZ, g_installPath + L"\\uninstall.exe");
    writeRegistry(hkey, L"Publisher", REG_SZ, L"Private Internet Access, Inc.");
    writeRegistry(hkey, L"DisplayVersion", REG_SZ, L"" PIA_VERSION_LITERAL);
    writeRegistry(hkey, L"DisplayIcon", REG_EXPAND_SZ, g_installPath + L"\\" BRAND_CODE "-client.exe");
    writeRegistry(hkey, L"URLInfoAbout", REG_SZ, L"https://www.privateinternetaccess.com/");
    if (g_installationSize)
        writeRegistry(hkey, L"EstimatedSize", (DWORD)(g_installationSize / 1024));
    writeRegistry(hkey, L"NoModify", 1);

    // Don't write an uninstall entry for this, it has a specific cleanup task.
    return false;
}

WriteUrlHandlerRegistryTask::WriteUrlHandlerRegistryTask(utf16ptr clientPath)
    : RegistryKeyTask{g_urlHandlerRegistryKey}, _clientPath{clientPath}
{
    assert(_clientPath);    // Ensured by caller
}

bool WriteUrlHandlerRegistryTask::updateKey(HKEY hkey)
{
    LOG("Writing URL handler");

    // Refer to https://docs.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa767914(v=vs.85)
    writeRegistry(hkey, nullptr, REG_SZ, L"" PIA_PRODUCT_NAME);
    writeRegistry(hkey, L"URL Protocol", REG_SZ, L"");  // This is supposed to be empty per MSDN

    HKEY defIcon = openSubkey(hkey, L"DefaultIcon");
    if(defIcon)
    {
        // Use first icon from client executable
        std::wstring iconPathIdx{L"\""};
        iconPathIdx += _clientPath;
        iconPathIdx += L"\",-1";
        writeRegistry(defIcon, nullptr, REG_SZ, iconPathIdx.c_str());
        ::RegCloseKey(defIcon);
    }

    HKEY shellOpenCommand = openSubkey(hkey, L"shell\\open\\command");
    if(shellOpenCommand)
    {
        std::wstring command{L"\""};
        command += _clientPath;
        command += L"\" \"%1\"";
        writeRegistry(shellOpenCommand, nullptr, REG_SZ, command.c_str());
        ::RegCloseKey(shellOpenCommand);
    }

    return true;
}

#endif // INSTALLER

RemoveRegistryKeyTask::RemoveRegistryKeyTask(std::wstring rootedPath)
    : _rootedPath{std::move(rootedPath)}, _splitPath{nullptr, nullptr}
{
    _splitPath = splitRegistryRootedPath(_rootedPath.c_str());
}

void RemoveRegistryKeyTask::execute()
{
    // This is possible; a future version could theoretically write a path that
    // we don't understand.  We'll just have to skip this key if it happens.
    if(!_splitPath.first || !_splitPath.second)
    {
        LOG("Can't install registry key %ls, path not valid", _rootedPath.c_str());
        return;
    }

    HKEY hkey{nullptr};
    LSTATUS err = ::RegOpenKeyExW(_splitPath.first, _splitPath.second, 0,
                                  KEY_ALL_ACCESS, &hkey);
    if(err == ERROR_NOT_FOUND)
    {
        LOG("Registry key does not exist: %ls", _splitPath.second);
        return;
    }
    else if(err)
    {
        LOG("Failed to open registry key %ls (%d)", _splitPath.second, err);
        return;
    }

    // Back up the key
    _existingKeyBackup.backup(hkey, _rootedPath.c_str());

    // Close the key and delete it
    ::RegCloseKey(hkey);

    LOG("Removing registry key: %ls", _rootedPath.c_str());
    if(LSTATUS err = ::RegDeleteTreeW(_splitPath.first, _splitPath.second))
    {
        LOG("Failed to delete registry key %ls (%d)", _rootedPath.c_str(), err);
    }
}

void RemoveRegistryKeyTask::rollback()
{
    // If we backed the key up, restore it.  If it didn't exist, this does
    // nothing.
    _existingKeyBackup.restore(_splitPath.first, _splitPath.second);
}

#ifdef UNINSTALLER

void RemoveUninstallRegistryTask::execute()
{
    auto splitPath = splitRegistryRootedPath(g_uninstallRegistryKey);
    LOG("Removing uninstall entry");
    if (LSTATUS err = RegDeleteTreeW(splitPath.first, splitPath.second))
    {
        LOG("Failed to delete uninstall entry (%d)", err);
    }
}

void RemoveRunRegistryTask::execute()
{
    LOG("Removing run entries");
    for (const auto& root : { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER })
    {
        if (LSTATUS err = RegDeleteKeyValueW(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"" PIA_PRODUCT_NAME))
        {
            if (err != ERROR_FILE_NOT_FOUND)
            {
                LOG("Unable to delete run entry (%d)", err);
                continue;
            }
        }
    }
}

#endif // UNINSTALLER
