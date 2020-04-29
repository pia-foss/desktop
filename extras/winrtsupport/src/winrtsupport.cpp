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

#include "common.h"
#line SOURCE_FILE("winrtsupport.cpp")

#include <QStringList>
#include "winrtsupport.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

#pragma comment(lib, "windowsapp.lib")

QStringView hstrToQstrView(const winrt::hstring &str)
{
    return QStringView{str.data(), static_cast<int>(str.size())};
}

QString hstrToQstr(const winrt::hstring &str)
{
    return QString::fromWCharArray(str.data(), static_cast<int>(str.size()));
}

winrt::hstring qstrToHstr(const QString &str)
{
    static_assert(sizeof(wchar_t) == sizeof(ushort));
    return winrt::hstring{reinterpret_cast<const wchar_t*>(str.utf16())};
}

// WinRT throws hresult_error objects, which aren't std::exceptions.  Throw
// std::exceptions to callers instead.
template<class Func_t>
auto convertExceptions(Func_t func) -> decltype(func())
{
    try
    {
        return func();
    }
    catch(const winrt::hresult_error &err)
    {
        QString msg = QStringLiteral("HRESULT: %1 - %2")
            .arg(static_cast<unsigned long>(err.code()))
            .arg(hstrToQstrView(err.message()));
        throw std::runtime_error{msg.toUtf8().data()};
    }
}

void initWinRt()
{
    convertExceptions([]{
        winrt::init_apartment();
    });
}

// Iterate over all apps in a family
template <class Func_t>
void forEachApp(const QString &family, Func_t func)
{
    return convertExceptions([&]{
        winrt::Windows::Management::Deployment::PackageManager packageMgr;
        auto packages = packageMgr.FindPackagesForUser({}, qstrToHstr(family));

        for(const auto &pkg : packages)
        {
            auto familyName = hstrToQstr(pkg.Id().FamilyName());
            auto appsAsync = pkg.GetAppListEntriesAsync();
            auto appEntries = appsAsync.get();
            for(const auto &appEntry : appEntries)
                func(appEntry);
        }
    });
}

// Return all apps for a family
// This typically returns only one app per family
// But we have one known exception in the case of winCommsApps ("Windows Communications Apps") which has a Mail and
// a Calendar app.
// All the apps for a family are ultimately coalesced into a single "app" (see getUwpApps()) for that family
// with a name that's a combination of their names, e.g "Mail, Calendar"
auto findAllApps(const QString &family) -> std::vector<winrt::Windows::ApplicationModel::Core::AppListEntry>
{
    std::vector<winrt::Windows::ApplicationModel::Core::AppListEntry> apps;

    forEachApp(family, [&apps](const auto &appEntry)
    {
        apps.push_back(std::move(appEntry));
    });

    // The order of apps in a package typically doesn't matter
    // However by reversing the order here we ensure that for the "windowscommunicationsapps" family
    // the name of the coalesced app will be "Mail, Calendar" and the Mail app icon will be used.
    std::reverse(apps.begin(), apps.end());

    return apps;
}

// Load an app's display name - only locates apps for the current user; used by
// client in user session.
//
// The display name is the concatenation of all app names in a family
// though in most cases there's only one app per family.
QString loadAppDisplayName(const QString &family)
{
    return convertExceptions([&]{
        QStringList appNames;
        auto apps = findAllApps(family);

        // If a family has multiple apps - concatenate the names together
        // e.g the winCommsApps family will be represented by a single
        // app with the name "Mail, Calendar".
        for(const auto &app : apps)
        {
            auto appDisplayInfo = app.DisplayInfo();
            appNames << hstrToQstr(appDisplayInfo.DisplayName());
        }

        return appNames.join(QStringLiteral(", "));
    });
}

// Return all Uwp Apps for the current user.
//
// If an app family has multiple apps (e.g winCommsAppsFamily) we still treat it as one app
// and we concatenate the names of each app together to form the name.
//
// We can get away with treating all apps in a family as a single app as there's one app manifest file
// per family - and the split tunnel includes/excludes all the app executables in that manifest.
std::vector<EnumeratedUwpApp> getUwpApps()
{
   return convertExceptions([&]{
        winrt::Windows::Management::Deployment::PackageManager packageMgr;

        auto pkgEnumerable = packageMgr.FindPackagesForUser({});

        std::vector<EnumeratedUwpApp> apps;
        for(const auto &pkg : pkgEnumerable)
        {
            auto pkgId = pkg.Id();
            auto appsAsync = pkg.GetAppListEntriesAsync();
            auto appEntries = appsAsync.get();
            for(const auto &appEntry : appEntries)
            {
                EnumeratedUwpApp app{};
                auto appDisplayInfo = appEntry.DisplayInfo();
                app.appPackageFamily = hstrToQstr(pkgId.FamilyName());
                app.displayName = loadAppDisplayName(app.appPackageFamily);

                apps.push_back(std::move(app));

                // Early-exit after finding the first app in a family
                // We only need one app per-family as already explained
                break;
            }
        }

        return apps;
    });
}

// Find the first app for a package family.  Note that this only loads packages
// for the current user, it is used by the client to inspect apps in the user's
// session.
std::optional<winrt::Windows::ApplicationModel::Core::AppListEntry> findApp(const QString &family)
{
    auto apps = findAllApps(family);

    if(!apps.empty())
        // Only get the first app for a family.
        // There's normally only one app per family.
        // One exception is the winCommsApps family which has multiple apps
        // But we still treat it as a single app here (the split tunnel will exclude/include
        // all apps in a family as the executables appear in the same manifest file)
        // The name of the app is a combination of all apps in the family, e.g "Mail, Calendar"
        return {apps.front()};
    else
        return {};
}

// Load an app's icon - only locates apps for the current user; used by client
// in user session.  This returns the raw image file data.
std::vector<std::uint8_t> loadAppIcon(const QString &family, float width,
                                      float height)
{
    return convertExceptions([&]{
        auto app = findApp(family);
        if(app)
        {
            auto appDisplayInfo = app->DisplayInfo();
            auto logoStreamRef = appDisplayInfo.GetLogo(winrt::Windows::Foundation::Size{width, height});
            auto logoStream = logoStreamRef.OpenReadAsync().get();
            winrt::Windows::Storage::Streams::DataReader logoStreamReader{logoStream};
            logoStreamReader.LoadAsync(static_cast<std::uint32_t>(logoStream.Size())).get();
            std::vector<std::uint8_t> logoData;
            logoData.resize(static_cast<std::size_t>(logoStreamReader.UnconsumedBufferLength()));
            if(!logoData.empty())
            {
                logoStreamReader.ReadBytes({logoData.data(),
                                            logoData.data() + logoData.size()});
                return logoData;
            }
        }

        return std::vector<std::uint8_t>{};
    });
}

// Get the installation directories for all packages in a family (requires admin
// rights).  Used by the daemon to inspect the executables or WWA apps launched
// by apps in a package family.
std::vector<QString> adminGetInstallDirs(const QString &family)
{
    return convertExceptions([&]{
        winrt::Windows::Management::Deployment::PackageManager packageMgr;
        // Get the first app for this family arbitrarily (by getting the packages,
        // taking the first one, then getting the apps for that package).
        auto packages = packageMgr.FindPackages(qstrToHstr(family));

        std::vector<QString> installationDirs;
        for(const auto &pkg : packages)
            installationDirs.push_back(hstrToQstr(pkg.InstalledLocation().Path()));

        return installationDirs;
    });
}

const WinRtSupportEntryPoints _entryPoints
{
    &initWinRt,
    &getUwpApps,
    &loadAppDisplayName,
    &loadAppIcon,
    &adminGetInstallDirs
};

extern "C" __declspec(dllexport) const WinRtSupportEntryPoints *getWinRtEntryPoints()
{
    return &_entryPoints;
}
