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

#include "common.h"
#line SOURCE_FILE("winrtsupport.cpp")

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

std::vector<EnumeratedUwpApp> getUwpApps()
{
    return convertExceptions([]{
        winrt::Windows::Management::Deployment::PackageManager packageMgr;

        auto pkgEnumerable = packageMgr.FindPackagesForUser({});

        std::vector<EnumeratedUwpApp> apps;
        for(auto &&pkg : pkgEnumerable)
        {
            auto pkgId = pkg.Id();
            winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::ApplicationModel::Core::AppListEntry>> appsAsync = pkg.GetAppListEntriesAsync();
            winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::ApplicationModel::Core::AppListEntry> appEntries = appsAsync.get();
            for(const auto &appEntry : appEntries)
            {
                EnumeratedUwpApp app{};
                auto appDisplayInfo = appEntry.DisplayInfo();
                app.displayName = hstrToQstr(appDisplayInfo.DisplayName());
                app.appPackageFamily = hstrToQstr(pkgId.FamilyName());
                //app.installedLocation = hstrToQstr(pkg.InstalledLocation().Path());

                apps.push_back(std::move(app));
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
    winrt::Windows::Management::Deployment::PackageManager packageMgr;
    // Get the first app for this family arbitrarily (by getting the packages,
    // taking the first one, then getting the apps for that package).
    auto packages = packageMgr.FindPackagesForUser({}, qstrToHstr(family));
    // The for...break looks odd but it's a simple way to get the first item
    // from a winrt collection or fall back to a default
    for(const auto &pkg : packages)
    {
        auto appsAsync = pkg.GetAppListEntriesAsync();
        auto appEntries = appsAsync.get();
        for(const auto &appEntry : appEntries)
            return {appEntry};
        break;
    }

    return {};
}

// Load an app's display name - only locates apps for the current user; used by
// client in user session.
QString loadAppDisplayName(const QString &family)
{
    return convertExceptions([&]{
        auto app = findApp(family);
        if(app)
        {
            auto appDisplayInfo = app->DisplayInfo();
            return hstrToQstr(appDisplayInfo.DisplayName());
        }

        return QString{};
    });
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
