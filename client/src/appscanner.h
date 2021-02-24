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
#line HEADER_FILE("appscanner.h")


#include "clientsettings.h"
#include <QQuickImageProvider>


#ifndef APPSCANNER_H
#define APPSCANNER_H

class AppScanner : public QObject {
    Q_OBJECT

public:
    static std::unique_ptr<AppScanner> create();

public:
    virtual void scanApplications () = 0;

signals:
    void applicationScanComplete(const QJsonArray &applications);
};

class DummyAppIconProvider: public QQuickImageProvider
{
public:
    DummyAppIconProvider()
               : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        Q_UNUSED(id);
        int width = 100;
        int height = 50;

        if (size)
            *size = QSize(width, height);
        QPixmap pixmap(requestedSize.width() > 0 ? requestedSize.width() : width,
                       requestedSize.height() > 0 ? requestedSize.height() : height);
        pixmap.fill(QColor(QStringLiteral("yellow")).rgba());
        return pixmap;
    }
};

class DummyAppScanner : public AppScanner {
    Q_OBJECT
public:
    virtual void scanApplications () override;
};



#endif
