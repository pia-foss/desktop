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

#include "registry.h"
#include "file.h"
#include "brand.h"

static const wchar_t g_uninstallRegistryKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" BRAND_WINDOWS_PRODUCT_GUID;

std::vector<std::wstring> loadAllUserRegistryHives()
{
    HKEY key;
    if (LSTATUS err = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList", 0, KEY_READ, &key))
    {
        LOG("Failed to get list of profiles (%d)", err);
        return {};
    }
    std::vector<std::wstring> sids;
    for (DWORD index = 0; ; index++)
    {
        std::wstring name;
        name.resize(MAX_PATH, 0);
        DWORD len = name.size();
        if (LSTATUS err = RegEnumKeyExW(key, index, &name[0], &len, NULL, NULL, NULL, NULL))
        {
            if (err == ERROR_NO_MORE_ITEMS)
                return sids;
            LOG("Failed to retrieve profile %d (%d)", err);
            continue;
        }
        name.resize(len, 0);
        HKEY subkey;
        if (LSTATUS err = RegOpenKeyW(HKEY_USERS, name.c_str(), &subkey))
        {
            std::wstring path;
            path.resize(MAX_PATH, 0);
            DWORD len = path.size() * 2, type;
            if (LSTATUS err = RegGetValueW(key, name.c_str(), L"ProfileImagePath", RRF_RT_ANY, &type, &path[0], &len))
            {
                LOG("Failed to get profile path for %s (%d)", name, err);
                continue;
            }
            path.resize(len / 2 - 1, 0);
            path += L"\\ntuser.dat";
            if (!PathFileExistsW(path.c_str()))
                continue;
            if (LSTATUS err = RegLoadKeyW(HKEY_USERS, name.c_str(), path.c_str()))
            {
                LOG("Unable to load registry hive for %s (%d)", name, err);
                continue;
            }
        }
        else
            RegCloseKey(subkey);
        sids.emplace_back(std::move(name));
    }
    RegCloseKey(key);
    return sids;
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
        LOG("Unable to write registry value %s (%d)", name, err);
}
static void writeRegistry(HKEY hkey, utf16ptr name, DWORD type, utf16ptr value)
{
    writeRegistry(hkey, name, type, value.ptr, value.ptr ? (value.length() + 1) * 2 : 0);
}
static void writeRegistry(HKEY hkey, utf16ptr name, DWORD value)
{
    writeRegistry(hkey, name, REG_DWORD, &value, sizeof(DWORD));
}

void WriteUninstallRegistryTask::execute()
{
    LOG("Writing uninstall entry");

    HKEY hkey;
    DWORD disposition;
    if (LSTATUS err = RegCreateKeyExW(HKEY_LOCAL_MACHINE, g_uninstallRegistryKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, &disposition))
    {
        LOG("Failed to open registry key (%d)", err);
    }
    else
    {
        if (disposition == REG_OPENED_EXISTING_KEY)
        {
            _existingKeyBackup = createBackupFile();
            if (!_existingKeyBackup.empty())
            {
                DeleteFileW(_existingKeyBackup.c_str());
                if (err = RegSaveKeyExW(hkey, _existingKeyBackup.c_str(), NULL, REG_LATEST_FORMAT))
                {
                    LOG("Unable to backup registry key (%d)", err);
                }
                if (!PathFileExistsW(_existingKeyBackup.c_str()))
                    _existingKeyBackup.clear();
            }
        }
        // Translation note - product and company names are not translated.
        writeRegistry(hkey, L"DisplayName", REG_SZ, L"" PIA_PRODUCT_NAME);
        writeRegistry(hkey, L"InstallLocation", REG_EXPAND_SZ, g_installPath);
        writeRegistry(hkey, L"UninstallString", REG_EXPAND_SZ, g_installPath + L"\\uninstall.exe");
        writeRegistry(hkey, L"Publisher", REG_SZ, L"Private Internet Access, Inc.");
        writeRegistry(hkey, L"DisplayVersion", REG_SZ, L"" PIA_VERSION);
        writeRegistry(hkey, L"DisplayIcon", REG_EXPAND_SZ, g_installPath + L"\\" BRAND_CODE "-client.exe");
        writeRegistry(hkey, L"URLInfoAbout", REG_SZ, L"https://www.privateinternetaccess.com/");
        if (g_installationSize)
            writeRegistry(hkey, L"EstimatedSize", (DWORD)(g_installationSize / 1024));
        writeRegistry(hkey, L"NoModify", 1);
        RegCloseKey(hkey);
    }
}

void WriteUninstallRegistryTask::rollback()
{
    if (LSTATUS err = RegDeleteTreeW(HKEY_LOCAL_MACHINE, g_uninstallRegistryKey))
    {
        LOG("Failed to delete uninstall entry (%d)", err);
    }
    if (!_existingKeyBackup.empty())
    {
        LOG("Restoring uninstall entry");
        HKEY hkey;
        if (LSTATUS err = RegCreateKeyExW(HKEY_LOCAL_MACHINE, g_uninstallRegistryKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL))
            LOG("Failed to create uninstall entry (%d)", err);
        else
        {
            if (LSTATUS err = RegRestoreKeyW(hkey, _existingKeyBackup.c_str(), REG_FORCE_RESTORE))
                LOG("Failed to restore uninstall entry (%d)", err);
            RegCloseKey(hkey);
        }
    }
}

#endif // INSTALLER

#ifdef UNINSTALLER

void RemoveUninstallRegistryTask::execute()
{
    LOG("Removing uninstall entry");
    if (LSTATUS err = RegDeleteTreeW(HKEY_LOCAL_MACHINE, g_uninstallRegistryKey))
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
