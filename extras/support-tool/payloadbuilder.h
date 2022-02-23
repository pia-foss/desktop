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

#ifndef PAYLOADBUILDER_H
#define PAYLOADBUILDER_H

#include <QObject>
#include <QByteArray>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>
#include <QScopedPointer>

// The entire payload is in a single folder. The current CrashLab implementation
// doesn't depend on this, but it's better to keep the name consistent for future
// use
const QString PAYLOAD_ROOT = QStringLiteral("pia_files");

// Name of the payload file
const QString PAYLOAD_FILE = QStringLiteral("payload.zip");

// Log sizes are restricted to 5 mb by the logging mechanism.
// But to be on the safer side, ignore all files above 6 mb
const qint64 FILE_SIZE_LIMIT = 6000000;

class PayloadBuilder: public QObject
{
    Q_OBJECT
private:
    QScopedPointer<QFile> _combinedLogFile;
    bool _started = false;

    // Target temp dir where we build the payload
    QScopedPointer<QTemporaryDir> _targetDir;

    // A bute array containing the contents of the zip file for uploading
    QByteArray _payloadZipContent;

    // Returns a full filesystem path for a given relative file path
    // For example, if "file" is "client-crash/bar.dmp"
    // - if the current target dir is /tmp/foo
    // - current payload is /tmp/foo/payload
    // - Final file is "/tmp/foo/payload/client-crash/bar.dmp"
    // - This function ensures that client-crash dir is created
    //
    // We can use _targetDir->filePath() instead of this but this function ensures
    // the directory exists
    //
    // Also, it's important to note that QDir's absoluteFilePath normalizes
    // dir seprators, so client-crash/bar.dmp becomes client-crash\bar.dmp on windows
    QString payloadPath (const QString &fileName) {
        QDir payloadDir(_targetDir->filePath(PAYLOAD_ROOT));
        QFileInfo fi(payloadDir.absoluteFilePath(fileName));
        if(!fi.dir().exists()) {
            qDebug () << "Creating dir" << fi.dir().absolutePath();
            fi.dir().mkpath(".");
        }
        return fi.absoluteFilePath();
    }
    void addFileToPayload(const QString &sourcePath, const QString &targetPath);

public:
    explicit PayloadBuilder(QObject *parent = nullptr);

    Q_INVOKABLE void start ();
    Q_INVOKABLE void addLogFile(const QString &fullPath);
    Q_INVOKABLE void addClientDumpFile (const QString &fullPath);
    Q_INVOKABLE void addDaemonDumpFile (const QString &fullPath);
    Q_INVOKABLE bool finish (const QString &copyToPath = "");

    // Add any misc file (currently used for diagnostics.txt)
    Q_INVOKABLE void addFile (const QString &fullPath);

    Q_INVOKABLE QByteArray payloadZipContent() const;
signals:

public slots:
};

#endif // PAYLOADBUILDER_H
