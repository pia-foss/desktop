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

#include <common/src/common.h>
#include "appscanner.h"
#include "clientsettings.h"

#include <QQmlApplicationEngine>


#line HEADER_FILE("splittunnelmanager.h")

#ifndef SPLITTUNNELMANAGER_H
#define SPLITTUNNELMANAGER_H



class SplitTunnelManager : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<AppScanner> _appScanner;
    QJsonArray _scannedApplications;
    bool _needsScan = true;
    bool _scanActive = false;

public:
    static void installImageHandler (QQmlApplicationEngine *engine);
    explicit SplitTunnelManager();
    Q_INVOKABLE void scanApplications (bool force = false);
    Q_INVOKABLE QString getNameFromPath(const QString &path) const;
    Q_INVOKABLE bool validateCustomPath(const QString &path) const;
    Q_INVOKABLE QString normalizeSubnet(const QString &path) const;
    // On Windows only, read the target of a shell link.  Returns an empty
    // string on other platforms.  (See SplitTunnelRule::linkTarget.)
    Q_INVOKABLE QString readWinLinkTarget(const QString &path) const;

    Q_PROPERTY(QJsonArray scannedApplications READ getScannedApplications NOTIFY applicationListChanged)
    Q_PROPERTY(bool scanActive READ getScanActive NOTIFY scanActiveChanged)
    Q_PROPERTY(QString linuxNetClsPath READ getLinuxNetClsPath CONSTANT)
    Q_PROPERTY(QString macWebkitFrameworkPath READ getMacWebkitFrameworkPath CONSTANT)

    QJsonArray getScannedApplications() const
    {
        return _scannedApplications;
    }

    bool getScanActive ()  const {
        return _scanActive;
    }

    QString getLinuxNetClsPath() const;

    QString getMacWebkitFrameworkPath () const;

protected:
    void applicationScanCompleted (const QJsonArray &applications);

signals:
    void applicationListChanged(QJsonArray scannedApplications);
    void scanActiveChanged(bool scanActive);

private:
    nullable_t<QJsonArray> _splitTunnelErrors;
};

#endif
