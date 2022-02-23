// Copyright (c) 2022 Private Internet Access, Inc.
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
#line SOURCE_FILE("win_winrtloader.cpp")

#include "win_winrtloader.h"
#include "brand.h"
#include <VersionHelpers.h>

const QString uwpPathPrefix = QStringLiteral("uwp:");

WinRTSupport::WinRTSupport()
    : _pEntryPoints{nullptr}
{
    if(::IsWindows10OrGreater())
    {
        const QString modName = QStringLiteral(BRAND_CODE "-winrtsupport.dll");
        _getWinRtEntryPointsAddr = ProcAddress{modName, QByteArrayLiteral("getWinRtEntryPoints")};

        // This should succeed on Windows 10, and all function pointers should
        // be valid
        auto pGetEntryPoints = reinterpret_cast<WinRtSupportEntryPoints*(*)()>(_getWinRtEntryPointsAddr.get());
        if(pGetEntryPoints)
            _pEntryPoints = pGetEntryPoints();
        Q_ASSERT(_pEntryPoints);
        Q_ASSERT(_pEntryPoints->pInitWinRt);
        Q_ASSERT(_pEntryPoints->pGetUwpApps);
        Q_ASSERT(_pEntryPoints->pLoadAppDisplayName);
        Q_ASSERT(_pEntryPoints->pLoadAppIcon);
        Q_ASSERT(_pEntryPoints->pAdminGetInstallDirs);

        // For paranoia, ensure we load everything or nothing
        if(!_pEntryPoints || !_pEntryPoints->pInitWinRt ||
            !_pEntryPoints->pGetUwpApps || !_pEntryPoints->pLoadAppDisplayName ||
            !_pEntryPoints->pLoadAppIcon || !_pEntryPoints->pAdminGetInstallDirs)
        {
            qWarning() << "Failed to load WinRT support:" << SystemError{HERE};
            _pEntryPoints = nullptr;
        }
    }
    else
    {
        qInfo() << "Not loading WinRT support on OS older than Windows 10.";
    }
}

void WinRTSupport::initWinRt() const
{
    if(_pEntryPoints)
    {
        Q_ASSERT(_pEntryPoints->pInitWinRt); // Class invariant
        try
        {
            _pEntryPoints->pInitWinRt();
        }
        catch(const std::exception &ex)
        {
            qWarning() << "Exception initializing WinRT:" << QLatin1String{ex.what()};
        }
    }
}

std::vector<EnumeratedUwpApp> WinRTSupport::getUwpApps() const
{
    if(_pEntryPoints)
    {
        Q_ASSERT(_pEntryPoints->pGetUwpApps); // Class invariant
        try
        {
            return _pEntryPoints->pGetUwpApps();
        }
        catch(const std::exception &ex)
        {
            qWarning() << "Exception loading UWP apps:" << QLatin1String{ex.what()};
        }
    }
    return {};
}

QString WinRTSupport::loadAppDisplayName(const QString &family) const
{
    if(_pEntryPoints)
    {
        Q_ASSERT(_pEntryPoints->pLoadAppDisplayName); // Class invariant
        try
        {
            return _pEntryPoints->pLoadAppDisplayName(family);
        }
        catch(const std::exception &ex)
        {
            qWarning() << "Exception loading UWP app display name for" << family
                << ":" << QLatin1String{ex.what()};
        }
    }
    return {};
}

std::vector<std::uint8_t> WinRTSupport::loadAppIcon(const QString &family,
                                                    float width, float height) const
{
    if(_pEntryPoints)
    {
        Q_ASSERT(_pEntryPoints->pLoadAppIcon); // Class invariant
        try
        {
            return _pEntryPoints->pLoadAppIcon(family, width, height);
        }
        catch(const std::exception &ex)
        {
            qWarning() << "Exception loading UWP app icon for" << family
                << ":" << QLatin1String{ex.what()};
        }
    }
    return {};
}

std::vector<QString> WinRTSupport::adminGetInstallDirs(const QString &family) const
{
    if(_pEntryPoints)
    {
        Q_ASSERT(_pEntryPoints->pAdminGetInstallDirs); // Class invariant
        try
        {
            return _pEntryPoints->pAdminGetInstallDirs(family);
        }
        catch(const std::exception &ex)
        {
            qWarning() << "Exception loading UWP app install dirs for" << family
                << ":" << QLatin1String{ex.what()};
        }
    }
    return {};
}

const WinRTSupport &getWinRtSupport()
{
    static const WinRTSupport _support;
    return _support;
}

WinRTLoader::WinRTLoader()
{
    _mtaWorkerThread.invokeOnThread([]()
    {
        getWinRtSupport().initWinRt();
    });
}

QString WinRTLoader::loadAppDisplayName(const QString &family)
{
    QString displayName;
    _mtaWorkerThread.invokeOnThread([&]()
    {
        displayName = getWinRtSupport().loadAppDisplayName(family);
    });
    return displayName;
}

std::vector<std::uint8_t> WinRTLoader::loadAppIcon(const QString &family, float width, float height)
{
    std::vector<std::uint8_t> iconData;
    _mtaWorkerThread.invokeOnThread([&]()
    {
        iconData = getWinRtSupport().loadAppIcon(family, width, height);
    });
    return iconData;
}

std::vector<QString> WinRTLoader::adminGetInstallDirs(const QString &family)
{
    std::vector<QString> dirs;
    _mtaWorkerThread.invokeOnThread([&]()
    {
        dirs = getWinRtSupport().adminGetInstallDirs(family);
    });
    return dirs;
}

WinRTLoader &getWinRtLoader()
{
    static WinRTLoader loader;
    return loader;
}
