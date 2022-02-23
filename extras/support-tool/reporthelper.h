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

#include "payloadbuilder.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QPalette>
#include "brand.h"

#ifndef REPORTHELPER_H
#define REPORTHELPER_H
#pragma once


class ReportHelper: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE void sendPayload(const QByteArray &payloadContent, const QString &comment);
    Q_INVOKABLE void restartApp(bool safeMode);
    Q_INVOKABLE void exitReporter();
    Q_INVOKABLE void showFileInSystemViewer(const QString &fullPath);
    Q_INVOKABLE QString getFileName(const QString &fullPath);
    Q_INVOKABLE QStringList findCrashFiles(const QString &paths);
    static QStringList ensureFilesExist (const QStringList &paths);
    Q_INVOKABLE bool checkCrashesForBlacklist(const QStringList &paths);
    Q_PROPERTY(QString brandName READ brandName CONSTANT)
    Q_INVOKABLE QString getBrandParam(const QString &code);
    Q_PROPERTY(QPalette palette READ getPalette CONSTANT)

    // Set a pointer to the "params" object in the UI.
    static void setUIParams (QObject *params) {
        _uiParams = params;
    }

    static void showErrorMessage (const QString &errorMessage) {
        if(_uiParams) {
            _uiParams->setProperty("errorMessage", errorMessage);
        }
    }

    QString brandName () const {
        return QStringLiteral(BRAND_NAME);
    }

    QPalette getPalette() const;

private:
    QNetworkAccessManager _nm;
    QNetworkRequest _request;
    QNetworkReply *_reply;
    QString getUrl() const;

#ifdef Q_OS_WIN
    bool checkDumpFileAgainstBlacklist(const QString &path) const;
#endif
    static QObject *_uiParams;
signals:
    void uploadSuccess(QString code);
    void uploadFail (QString message);

private slots:
    void onUploadFinished();
    void onError(QNetworkReply::NetworkError err);
};

bool checkAutoRestart (const QString &settingsPath, const QString &clientCrashPath);
QString getEndpointOverride(const QString &overridePath);

#endif // REPORTHELPER_H
