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

#include "payloadbuilder.h"
#include "logging.h"
#include <QProcess>
#include <QUrl>
#include <QDateTime>
#include "path.h"
#include "reporthelper.h"

QByteArray PayloadBuilder::payloadZipContent() const
{
    return _payloadZipContent;
}

PayloadBuilder::PayloadBuilder(QObject *parent)
{

}

void PayloadBuilder::start()
{
    if(_started)
        qWarning () << "Started another payload before finishing an existing one";

    _started = true;

    _targetDir.reset(new QTemporaryDir());
    _targetDir->setAutoRemove(false);
    qDebug () << "Created temporary dir " << _targetDir->path();

    // Create a new file in the payload called "logs.txt" which will contain all logs added via addLogFile
    _combinedLogFile.reset(new QFile(payloadPath("logs.txt")));
    _combinedLogFile->open(QIODevice::WriteOnly);
    _payloadZipContent.clear();
}

bool PayloadBuilder::finish(const QString &copyToPath)
{
    _combinedLogFile->close();

#ifdef Q_OS_MACOS
    QString zipCommand = "zip";
#elif defined(Q_OS_WIN)
    QString zipCommand = Path::ExecutableDir / "zip.exe";
#elif defined (Q_OS_LINUX)
    QString zipCommand("/usr/bin/zip");
    if(!QFile::exists(zipCommand)) {
        ReportHelper::showErrorMessage(QStringLiteral("Missing 'zip' command. Please install the appropriate package."));
        return false;
    }
#endif

    QStringList args;
    args << PAYLOAD_FILE;
    args << "-r" << PAYLOAD_ROOT;

    QProcess zipProcess;
    zipProcess.setProgram(zipCommand);
    zipProcess.setArguments(args);
    zipProcess.setWorkingDirectory(_targetDir->path());
    zipProcess.start();
    zipProcess.waitForFinished();

    bool success = false;

    if(zipProcess.exitStatus() == QProcess::NormalExit) {
        QFile resultFile(_targetDir->filePath(PAYLOAD_FILE));
        qDebug() << (_targetDir->filePath(PAYLOAD_FILE));
        if(resultFile.exists()) {
            resultFile.open(QIODevice::ReadOnly);
            _payloadZipContent.append(resultFile.readAll());
            resultFile.close();

            // If we need to store a copy elsewhere (for save as zip) make a copy
            // but success is determined by whether we could copy or not.
            // even though the zip could be generated successfully
            if(copyToPath.length() > 0) {
                // Sometimes the file dialog can provide "file://" schema URLs.
                // To reliably convert them we need to make a new path

                QString copyTargetPath = copyToPath;
                if(copyToPath.startsWith("file://")) {
                     copyTargetPath = QUrl(copyToPath).toLocalFile();
                }

                if(!resultFile.copy(copyTargetPath)) {
                    qWarning () << "Unable to copy to " << copyTargetPath;
                }
                else {
                    success = true;
                }
            }
            else {
                success = true;
            }
        } else {
            qWarning () << "Payload zip file does not exist";
        }
    }
    else {
        qWarning () << "Zip return code" << zipProcess.exitCode();
    }

    if(!success)
    {
        qWarning () << "Zip process failed.";
        qWarning () << zipProcess.readAllStandardOutput();
        qWarning () << zipProcess.readAllStandardError();
    }

    // Flag that everything is cleaned up
    _started = false;

    return success;
}

void PayloadBuilder::addFile(const QString &fullPath)
{
    QFileInfo fi(fullPath);
    addFileToPayload(fullPath, fi.fileName());
}

void PayloadBuilder::addClientDumpFile(const QString &fullPath)
{
    QFileInfo fi(fullPath);
    // For the dump file, there's no way to tell exactly when the crash happened
    // from the dmp file itself.
    // Instead of including a separate manifest containing this meta data, for now
    // we can re-name the file to include the timestamp

    //
    // Also, some platforms (like Linux) doesn't always support timestamps on the
    // filesystems. So we include a random string in the filename so
    // there's always a valid filename. The first 5 characters of the filename
    // should be good enough
    addFileToPayload(fullPath, QStringLiteral("client-crash/%1-%2.dmp")
                     .arg(fi.fileName().left(5))
                     .arg(fi.birthTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))));
}

void PayloadBuilder::addDaemonDumpFile(const QString &fullPath)
{
    QFileInfo fi(fullPath);
    addFileToPayload(fullPath, QStringLiteral("daemon-crash/%1-%2.dmp")
                     .arg(fi.fileName().left(5))
                     .arg(fi.birthTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))));
}

void PayloadBuilder::addFileToPayload(const QString &sourcePath, const QString &targetName)
{
    if(!_started) {
        qWarning () << "Not started yet";
        return;
    }

    QString fullTargetPath = payloadPath(targetName);
    if(QFileInfo(targetName).size() > FILE_SIZE_LIMIT) {
        qWarning () << "Skipped large file " << targetName;
        return;
    }
    if(QFile::copy(sourcePath, fullTargetPath)) {
        qDebug () << "Added file: " << fullTargetPath;
    }
    else {
        qWarning() << "Unable to add file: " << fullTargetPath;
    }
}

void PayloadBuilder::addLogFile(const QString &fullPath)
{
    if(!_started) {
        qWarning () << "Not started yet";
        return;
    }

    qDebug () << "Adding log file with path: " << fullPath;

    QFileInfo fi(fullPath);
    if(fi.exists() && fi.isReadable()) {
        // Read file and write it into the combined log file along with the "PIA_PART" header
        _combinedLogFile->write((QStringLiteral("\n/PIA_PART/%1\n").arg(fi.fileName()).toUtf8()));

        if(fi.size() > FILE_SIZE_LIMIT) {
            _combinedLogFile->write(QStringLiteral("File Too large. Skipping \n").toUtf8());
            return;
        }

        QFile file(fi.filePath());
        file.open(QFile::ReadOnly);
        _combinedLogFile->write(file.readAll());
        file.close();
    }
    else {
        qWarning () << "Cannot add file" << fi.path();
    }

    if(QFile::exists(fullPath + oldFileSuffix)) {
        addLogFile(fullPath + oldFileSuffix);
    }
}
