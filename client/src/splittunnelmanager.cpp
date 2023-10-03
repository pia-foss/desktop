// Copyright (c) 2023 Private Internet Access, Inc.
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

#include <common/src/common.h>
#include "client.h"
#include "splittunnelmanager.h"
#include <common/src/builtin/path.h>
#include <common/src/semversion.h>

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QHostAddress>
#if defined(Q_OS_MAC)
#include "mac/mac_appscanner.h"
#include <kapps_core/src/mac/mac_constants.h>
#elif defined(Q_OS_WIN)
#include "win/win_appscanner.h"
#include <kapps_core/src/win/win_linkreader.h>
#include <VersionHelpers.h>
#elif defined(Q_OS_LINUX)
#include "linux/linux_appscanner.h"
#endif

void SplitTunnelManager::installImageHandler(QQmlApplicationEngine *engine)
{
#if defined(Q_OS_MAC)
    engine->addImageProvider(QStringLiteral("appicon"), new MacAppIconProvider);
#elif defined(Q_OS_WIN)
    engine->addImageProvider(QStringLiteral("appicon"), createWinAppIconProvider().release());
#else
    engine->addImageProvider(QStringLiteral("appicon"), new DummyAppIconProvider);
#endif
}

SplitTunnelManager::SplitTunnelManager()
{
    _appScanner = AppScanner::create();
    connect(_appScanner.get(), &AppScanner::applicationScanComplete, this, &SplitTunnelManager::applicationScanCompleted);
}

void SplitTunnelManager::scanApplications(bool force)
{
    if((force || _needsScan) && !_scanActive){
        _scanActive = true;
        emit scanActiveChanged(_scanActive);
        _appScanner->scanApplications();
    }
}

QString SplitTunnelManager::getNameFromPath(const QString &path) const
{
#if defined(Q_OS_MAC)
    return getMacAppName(path);
#elif defined(Q_OS_WIN)
    return getWinAppName(path);
#elif defined(Q_OS_LINUX)
    return getLinuxAppName(path);
#else
    return QStringLiteral("");
#endif
}

bool SplitTunnelManager::validateCustomPath(const QString &path) const
{
#if defined(Q_OS_LINUX)
  return validateLinuxCustomPath(path);
#elif defined(Q_OS_MAC)
  // Do not allow adding the VPN Application
  return !path.startsWith(Path::InstallationDir);
#else
  return true;
#endif
}

QString SplitTunnelManager::readWinLinkTarget(const QString &path) const
{
#if defined(Q_OS_WIN)
    try
    {
        kapps::core::WinLinkReader linkReader;
        const auto &pathStr{path.toStdWString()};
        if(!linkReader.loadLink(pathStr))
            throw Error{HERE, Error::Code::Unknown};
        return QString::fromStdWString(linkReader.getLinkTarget(pathStr));
    }
    catch(const std::exception &ex)
    {
        qWarning() << "Can't read target of link" << path << "-" << ex.what();
    }
#endif
    return {};
}

QString SplitTunnelManager::getLinuxNetClsPath() const
{
#if defined(Q_OS_LINUX)
    return Path::ParentVpnExclusionsFile.parent();
#else
    return {};
#endif
}

QString SplitTunnelManager::normalizeSubnet(const QString &subnet) const
{
    auto subnetPair = QHostAddress::parseSubnet(subnet);

    const QHostAddress &ipAddress = subnetPair.first;
    int prefixLength = subnetPair.second;

    if(!ipAddress.isNull())
        return QStringLiteral("%1/%2")
            .arg(ipAddress.toString())
            .arg(QString::number(prefixLength));
    else
        return {};
}

QString SplitTunnelManager::getMacWebkitFrameworkPath() const
{
#ifdef Q_OS_MAC
    return QString::fromStdString(webkitFrameworkPath);
#else
    return QStringLiteral("");
#endif
}

void SplitTunnelManager::applicationScanCompleted(const QJsonArray &applications)
{
    _scannedApplications = applications;
    _needsScan = false;
    _scanActive = false;
    emit scanActiveChanged(_scanActive);
    emit applicationListChanged(_scannedApplications);
}
