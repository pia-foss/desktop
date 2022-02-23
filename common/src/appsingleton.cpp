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

#include "appsingleton.h"
#include "brand.h"

#if defined(Q_OS_WIN)
#pragma comment(lib, "Kernel32.lib")
#include <Windows.h>
#include <psapi.h>
#include <array>
#elif defined(Q_OS_LINUX)
#include <unistd.h> // readlink()
#endif

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <algorithm>

const int RESOURCE_LENGTH_LIMIT = 4000;

bool AppSingleton::lockResourceShare()
{
    if(!_resourceShare.isAttached()) {
        qCritical() << "Attempting to lock resource share when not attached";
        return false;
    }
    return _resourceShare.lock();
}

bool AppSingleton::unlockResourceShare()
{
    return _resourceShare.unlock();
}

AppSingleton::AppSingleton()
    : _executablePath{QCoreApplication::applicationFilePath()},
      _pidShare{_executablePath},
      _resourceShare{_executablePath + QStringLiteral("_RESOURCE")}
{
    _resourceShare.create(RESOURCE_LENGTH_LIMIT);

    if(_resourceShare.error() == QSharedMemory::NoError || _resourceShare.error() == QSharedMemory::AlreadyExists) {
        if(!_resourceShare.attach()) {
            qWarning() << "Unable to attach to shared memory" << _resourceShare.errorString();
        }
    }
}

AppSingleton::~AppSingleton()
{
    if(_pidShare.isAttached())
        _pidShare.detach();
    if(_resourceShare.isAttached())
        _resourceShare.detach();
}

// Returns true if finds a process with the pid
// Returns false on any error or if unable
bool findRunningProcess (qint64 pid, const QString &processPath) {
    qInfo() << "This process:" << processPath;
#if defined(Q_OS_WIN)
    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                       PROCESS_VM_READ,
                                       FALSE, (DWORD)pid);
    std::array<wchar_t, MAX_PATH> szProcessName;
    if(hProcess != NULL) {
        GetModuleFileNameExW(hProcess, NULL, szProcessName.data(), szProcessName.size());

        // GetModuleFileName returns a file path with back slashes. We need to convert that
        // to forward slashes which is the parameter of processPath
        QString winProcName = QString::fromWCharArray(szProcessName.data())
                .replace(QStringLiteral("\\"), QStringLiteral("/"))
                .trimmed();

        CloseHandle(hProcess);

        qInfo() << "PID" << pid << "-" << winProcName;

        if(processPath == winProcName)
            return true;
        else
            qDebug () << "Found a process " << processPath << " running with PID " << pid << " instead.";
    } else
        qDebug () << "Unable to open process " << pid;
    return false;

#elif defined(Q_OS_MACOS)
    // For mac, use ps to get the executable for this PID
    QProcess ps;
    ps.setProgram(QStringLiteral("ps"));
    QStringList args;
    args << "-p" << QString::number(static_cast<qlonglong>(pid));
    args << "-o" << "comm=";
    ps.setArguments(args);
    ps.start();
    ps.waitForFinished();

    // ps -p <pid> -o comm=
    // Prints a full path to the running process. This should ideally be equal.
    QString stdout = QString::fromUtf8(ps.readAllStandardOutput()).trimmed();
    qInfo() << "PID" << pid << "-" << stdout;

    // processPath contains an absolute path to the process according to the `Path` class
    //
    // ps -p can return /opt/piavpn/bin/vpn-client or ./vpn-client
    // or even simply 'vpn-client' if app was restarted from support tool
    //
    // On mac, the file names are mostly consistent but there's no
    // solid guarantee that this will always return the full path


    QFileInfo procInfo(processPath), psInfo(stdout);
    // Ensure that the filenames are same for both the process and the output from `ps`
    // Also if the path is invalid, `fileName` will return an empty string. Check that this isn't empty.
    return (procInfo.fileName() == psInfo.fileName()) && !procInfo.fileName().isEmpty();
#elif defined(Q_OS_LINUX)
    // Linux process names are limited to 15 chars (16 including a terminating
    // null char).  Read /proc/####/exe to get the process executable.
    QString procExePath{QStringLiteral("/proc/%1/exe").arg(pid)};
    std::string procExe{};
    procExe.resize(2048);
    ssize_t exeLen = ::readlink(qPrintable(procExePath), &procExe[0], procExe.size());
    procExe.resize(std::max(ssize_t{0}, exeLen));
    QString procExeU16{QString::fromUtf8(procExe.c_str())};

    qInfo() << "PID" << pid << "-" << procExeU16;

    return processPath == procExeU16;
#endif

    return false;
}


qint64 AppSingleton::isAnotherInstanceRunning()
{
    qDebug () << "Checking for another instance";
    // at this point, we assume that the shared memory is attached.
    if(!_pidShare.create(sizeof(qint64))) {
        qWarning() << "Create failed with " << _pidShare.errorString();

        if(_pidShare.error() != QSharedMemory::AlreadyExists) {
            // We can't tell for sure. Prefer a false negative over a false positive
            return false;
        }
    }

    if(!_pidShare.isAttached()) {
        if(!_pidShare.attach()) {
            qWarning() << "Unable to attach to shared memory" << _pidShare.errorString();

            // Since we are unable to determine if another app is indeed running
            // assume it isn't. A false positive is worse than false negative.
            return false;
        }
    }

    _pidShare.lock();
    qint64 *data = reinterpret_cast<qint64*>(_pidShare.data());
    bool processFound = false;
    qDebug () << "Value of shared memory data is " << *data;
    if(*data > 0)
        processFound = findRunningProcess(*data, _executablePath);

    // If no other instance is found, write the current PID as the primary running application.
    if(!processFound)
        *data = QCoreApplication::applicationPid();

    _pidShare.unlock();

    if(processFound)
      return *data;
    else
        return -1;
}

void AppSingleton::setLaunchResource(QString url)
{
    if(lockResourceShare()) {
        char* data = reinterpret_cast<char*>(_resourceShare.data());
        strcpy(data, url.toUtf8().constData());
        unlockResourceShare();
    }
}

QString AppSingleton::getLaunchResource()
{
    QString result;
    if(lockResourceShare()) {
        result = QString::fromUtf8(reinterpret_cast<const char*>(_resourceShare.constData()));

        memset(_resourceShare.data(), 0, RESOURCE_LENGTH_LIMIT - 1);

        unlockResourceShare();
    }

    return result;
}

template class COMMON_EXPORT Singleton<AppSingleton>;
