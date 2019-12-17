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

#include "reporthelper.h"
#include "version.h"
#ifdef Q_OS_LINUX
#include <unistd.h>
#include "launcher/linux-launcher-cmds.h"
#endif
#include <QDebug>
#include <QHttpMultiPart>
#include <QFileInfo>
#include <QNetworkReply>
#include <QProcess>
#include <QGuiApplication>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include "path.h"
#include "brand.h"

#ifdef Q_OS_WIN
#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "DbgHelp.lib")
#include<Windows.h>
#include<DbgHelp.h>
#include<minidumpapiset.h>
#endif

QObject *ReportHelper::_uiParams;

void ReportHelper::sendPayload(const QByteArray &payloadContent, const QString &comment)
{
    // Set up the request
    QString url = getUrl("/api/v1/reports/upload");
    qDebug () << "Sending payload to URL: " << url << "Payload size: " << payloadContent.size();
    _request.setUrl(url);

    // Create a multipart uploader. We will delete this in `onUploadFinished`
    QHttpMultiPart *uploader = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    //
    // Create a file part from the zip file contents from PayloadBuilder
    //
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"payload\"; filename=\""+ PAYLOAD_FILE + "\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    filePart.setBody(payloadContent);

    //
    // Create parts for the version/comment/platform
    //
    QHttpPart verPart;
    verPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"version\""));
    verPart.setBody(PIA_VERSION);

    QHttpPart commentPart;
    commentPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"comments\""));
    commentPart.setBody(comment.toUtf8());

    QHttpPart brandPart;
    brandPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"brand\""));
    brandPart.setBody(QStringLiteral(BRAND_CODE).toUtf8());

    QHttpPart platformPart;
    platformPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"platform\""));
#if defined(Q_OS_MAC)
    platformPart.setBody("mac");
#elif defined(Q_OS_WIN)
    #if defined(Q_PROCESSOR_X86_64)
        platformPart.setBody("win-x64");
    #else
        platformPart.setBody("win-x86");
    #endif // defined(Q_PROCESSOR_X86_64)
#elif defined(Q_OS_LINUX)
    platformPart.setBody("linux-x64");
#endif

    uploader->append(verPart);
    uploader->append(brandPart);
    uploader->append(platformPart);
    uploader->append(commentPart);
    uploader->append(filePart);

    // Perform the post request
    _reply = _nm.post(_request, uploader);
    uploader->setParent(_reply);

    connect(_reply, &QNetworkReply::finished, this, &ReportHelper::onUploadFinished);
    connect(_reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            this, &ReportHelper::onError);
}

void ReportHelper::restartApp(bool safeMode)
{
    if(safeMode)
    {
        qDebug() << "Restarting client in safe mode";
    }

#ifdef Q_OS_LINUX
    int invokePipeFd = -1;
    if(_uiParams)
        invokePipeFd = _uiParams->property("invoke_pipe").toInt();
    if(invokePipeFd >= 0)
    {
        unsigned char command = safeMode ? StartClientSafe : StartClientNormal;
        ssize_t writeResult = ::write(invokePipeFd, &command, sizeof(command));
        if(writeResult == static_cast<ssize_t>(sizeof(command)))
        {
            qInfo() << "Signaled launcher to restart client on pipe" << invokePipeFd;
            return;
        }
    }
    // Otherwise, if we weren't given an invoke pipe, or if writing failed
    // somehow, restart it ourselves.
    qInfo() << "Starting client directly from support tool";
#endif

    QProcess app;
    app.setProgram(Path::ClientExecutable);
    if(safeMode)
      app.setArguments(QStringList() << QStringLiteral("--safe-mode"));

    app.startDetached();
    this->exitReporter();
}

void ReportHelper::exitReporter()
{
    QGuiApplication::quit();
}

void ReportHelper::showFileInSystemViewer(const QString &filePath)
{
#ifdef Q_OS_MAC
    QStringList args;
    args << QStringLiteral("-R");
    args << filePath;
    QProcess::execute(QStringLiteral("/usr/bin/open"), args);
#endif

#ifdef Q_OS_WIN
    QFileInfo fileInfo(filePath);
    QStringList args;
    if (!fileInfo.isDir())
        args += QLatin1String("/select,");
    args += QDir::toNativeSeparators(fileInfo.canonicalFilePath());
    QProcess::startDetached(QStringLiteral("explorer.exe"), args);
#endif

#ifdef Q_OS_LINUX
    QFileInfo fileInfo(filePath);
    if (QFile::exists(QStringLiteral("/usr/bin/xdg-open")))
        QProcess::startDetached(QStringLiteral("/usr/bin/xdg-open"), QStringList() << fileInfo.dir().path());
    else
        showErrorMessage(QStringLiteral("Missing xdg-open program to open file browser. Please install an appropriate package."));
#endif

}

QString ReportHelper::getFileName(const QString &fullPath)
{
    QFileInfo fi(fullPath);
    return fi.fileName();
}

// Find a list of crash files inside a given path
// TODO: possibly include only the most recent crashes
QStringList ReportHelper::findCrashFiles(const QString &path)
{
    QStringList result;
    QDir crashDir (path);
    if(!crashDir.exists() || path.length() == 0) {
        return result;
    }
    qDebug () << "Looking for crash files in"<< path;

    QDirIterator di(path, QStringList() << "*.dmp", QDir::Files);
    while (di.hasNext()) {
        result.append(di.next());
    }

    qDebug () << "Found crash files:" << result;

    return result;
}

// Go through a list of file paths and ignore ones where the files
// don't exist
QStringList ReportHelper::ensureFilesExist(const QStringList &paths)
{
    // We can either create a new stringlist of valid paths or
    // modify the existing one. Modifying in place had issues
    // with deleting values, and since there's no way to filter with a lambda
    // let's just create a new valid list instead
    QStringList validPaths;

    for(int i = 0; i < paths.size(); i++ ) {
        if(QFile::exists(paths[i])) {
            validPaths << paths[i];
        }
    }

    return validPaths;
}

bool ReportHelper::checkCrashesForBlacklist(const QStringList &paths)
{
#ifndef Q_OS_WIN
    return false;
#else
    if(paths.size() == 0)
        return false;

    QStringList validFiles = ensureFilesExist(paths);

    QString mostRecentFile;
    QDateTime mostRecentTimestamp(QDateTime::fromSecsSinceEpoch(0));
    for(const auto &path: paths) {
        QFileInfo fi(path);
        if(fi.birthTime() > mostRecentTimestamp) {
            mostRecentFile = path;
            mostRecentTimestamp = fi.birthTime();
        }
    }

    if(!mostRecentFile.isEmpty())
        return checkDumpFileAgainstBlacklist(mostRecentFile);
    else
        return false;
#endif
}

QString ReportHelper::getBrandParam(const QString &code)
{
    QJsonDocument params = QJsonDocument::fromJson(QStringLiteral(BRAND_PARAMS).toUtf8());

    if(params.isNull()) {
        qWarning () << "Unable to read brand JSON";
        return QStringLiteral("");
    }

    if(params.isObject()) {
        QJsonObject paramsObject = params.object();
        if(paramsObject.keys().contains(code)) {
            return paramsObject.value(code).toString();
        }

        qWarning () << "Unable to find param " << code;
        return QStringLiteral("");
    }

    qWarning () << "BRAND_PARAMS is not a valid JSON object.";
    return QStringLiteral("");
}


QString ReportHelper::getUrl(const QString &path) const
{
    QString base = QStringLiteral("https://clients.privateinternetaccess.com");

    return base.append(path);
}


#ifdef Q_OS_WIN
// returns true if crashing module is amongst a list of
// blacklisted modulesbool ReportHelper::checkDumpFileAgainstBlacklist(const QString &path) const
bool ReportHelper::checkDumpFileAgainstBlacklist(const QString &dmpPath) const
{
    QStringList blacklist;
    blacklist << "ig4icd64.dll"
              << "ig4icd32.dll"
              << "ig9icd64.dll"
              << "ig9icd32.dll"
              << "atio6axx.dll"
              << "nvoglv64.dll"
              << "atioglxx.dll";

    QFile file(dmpPath);
    file.open(QIODevice::ReadOnly);

    // if file is larger than 5 MB then it's too large to process.
    if(file.size() > 5000000) {
        return false;
    }
    QByteArray dmpData(file.readAll());

    PMINIDUMP_DIRECTORY dir;
    MINIDUMP_EXCEPTION_STREAM *es;
    MINIDUMP_MODULE_LIST *ml;
    ULONG streamSize;
    BOOL result;

    result = MiniDumpReadDumpStream(static_cast<VOID*>(dmpData.data()),
                                     ExceptionStream, &dir,
                                     reinterpret_cast<PVOID*>(&es), &streamSize);
    if(!result) {
        qWarning () << "Unable to read dump stream for ES";
        return false;
    }
    ULONG64 exceptionAddress = es->ExceptionRecord.ExceptionAddress;

    result = MiniDumpReadDumpStream(static_cast<VOID*>(dmpData.data()),
                                    ModuleListStream, &dir,
                                    reinterpret_cast<PVOID*>(&ml), &streamSize);
    if(!result) {
        qWarning () << "Unable to read module stream for ES";
        return false;
    }

    QString moduleName;
    for(ULONG32 i = 0; i < ml->NumberOfModules; ++i) {
        MINIDUMP_MODULE m = ml->Modules[i];
        if(exceptionAddress > m.BaseOfImage &&
                exceptionAddress < (m.BaseOfImage + m.SizeOfImage)) {

            int moduleNameRva = static_cast<int>(m.ModuleNameRva);

            ULONG32 *length = reinterpret_cast<ULONG32*>(dmpData.data() + moduleNameRva);
            char16_t *nameData = reinterpret_cast<char16_t*>(dmpData.data() + moduleNameRva + sizeof(ULONG32));

            // module path is "C:\\foo\\bar\\module.dll"
            if(moduleNameRva + sizeof(ULONG32) < dmpData.size() &&
               moduleNameRva + sizeof(ULONG32) + *length < dmpData.size())
            {
                QString modulePath = QString::fromUtf16(nameData, *length / sizeof(char16_t));
                // Returns -1 if there's no backslash, so we would take the
                // whole string in that case (-1 + 1 -> start idx 0)
                auto modNameIdx = modulePath.lastIndexOf('\\') + 1;
                moduleName = modulePath.mid(modNameIdx);
                qDebug () << "Found crashing module name: " << moduleName;
            }
            break;
        }
    }
    if(moduleName.isEmpty())
    {
        qWarning () << "Could not find module";
    }

    return blacklist.contains(moduleName);
}
#endif

void ReportHelper::onUploadFinished()
{
    QString data;

    QNetworkReply::NetworkError errCode = _reply->error();
    qDebug () << "Upload has finished with code: " << errCode;
    if(errCode == QNetworkReply::NoError) {
        data = QString::fromUtf8(_reply->readAll());
    }

    // Ensure we clear out _reply before we emit uploadSuccess since this might result in a
    _reply->close();
    _reply->deleteLater();
    _reply = nullptr;

    if(data.length() > 15) {
        qDebug () << "Suspicious output. Might be server error. Check server logs";
#ifdef _DEBUG
        qDebug () << data;
#endif
        emit uploadFail(QStringLiteral("Server error"));
    }
    else if(errCode == QNetworkReply::NoError) {
        qDebug() << "Upload Success";
        emit uploadSuccess(data);
    }
    else {
        // Network errors will have
        qDebug () << "Network error: " << errCode;
    }
}

void ReportHelper::onError(QNetworkReply::NetworkError err)
{
    qDebug () << "Network error " << err;
    emit uploadFail(_reply->errorString());
}

bool checkDisableHardwareGraphicsValue (const QString &settingsPath, bool *value) {
  QFileInfo sfi(settingsPath);
  if(sfi.exists() && sfi.isReadable()) {
    QFile settingsFile(sfi.filePath());
    settingsFile.open(QIODevice::ReadOnly);
    QJsonParseError error;
    QJsonDocument settings = QJsonDocument::fromJson(settingsFile.readAll(), &error);
    settingsFile.close();
    if(error.error != QJsonParseError::NoError) {
      qWarning () << "Unable to read json file:" << error.errorString() << error.offset;
      return false;
    }
    if(!settings.isObject()) {
      qWarning () << "Malformed json";
      return false;
    }
    QJsonValue disableHardwareGraphicsValue =  settings.object().take(QStringLiteral("disableHardwareGraphics"));

    if(disableHardwareGraphicsValue.isBool()) {
      *value = disableHardwareGraphicsValue.toBool(false);
      return true;
    }
  }

  return false;
}


bool checkAutoRestart(const QString &settingsPath, const QString &clientCrashPath)
{
  ReportHelper rh;

  qDebug () << "Checking if we need to auto-restart in safe-mode";
  QStringList clientCrashes = rh.findCrashFiles(clientCrashPath);
  if(rh.checkCrashesForBlacklist(clientCrashes)) {
    qDebug () << "Found blacklisted driver in crash";

    bool disableHardwareGraphicsValue = true;
    if(checkDisableHardwareGraphicsValue(settingsPath, &disableHardwareGraphicsValue)) {
      if(!disableHardwareGraphicsValue) {
        qDebug () << "Safe mode not enabled, force enabling it now";

        // Request a restart when the main loop is started
        QTimer::singleShot(500, []{
          ReportHelper rh;
          rh.restartApp(true);
        });

        return true;
      }
      else {
        qWarning () << "Safe mode is already enabled";
      }
    }
  }

  return false;
}
