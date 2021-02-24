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

#include "common.h"
#line HEADER_FILE("mac_appscanner.h")

#ifndef MAC_APPSCANNER_H
#define MAC_APPSCANNER_H

#include "../appscanner.h"
#include "thread.h"
#include <QThread>

QPixmap getIconForAppBundle(const QString &path, const QSize &size);

QString getMacAppName(const QString &path);

class MacAppIconProvider : public QQuickImageProvider
{
public:
    MacAppIconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};

class MacAppScanner : public AppScanner
{
    Q_OBJECT
public:
    explicit MacAppScanner();
    virtual void scanApplications () override;
private:
    RunningWorkerThread _workerThread;
};

#endif
