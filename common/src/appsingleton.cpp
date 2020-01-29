// Copyright (c) 2020 Private Internet Access, Inc.
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
#ifdef Q_OS_WIN
#pragma comment(lib, "Kernel32.lib")
#include <Windows.h>
#include <psapi.h>
#include <array>
#endif

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

AppSingleton::AppSingleton(const QString &executableName, QObject *parent) :
    QObject(parent),
    _pidShare(executableName),
    _executableName(executableName) {

}

AppSingleton::~AppSingleton()
{
    if(_pidShare.isAttached())
        _pidShare.detach();
}

// Returns true if finds a process with the pid
// Returns false on any error or if unable
bool findRunningProcess (qint64 pid, const QString &processName) {
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                       PROCESS_VM_READ,
                                       FALSE, (DWORD)pid);
    std::array<wchar_t, MAX_PATH> szProcessName;
    if(hProcess != NULL) {
        GetModuleFileNameExW(hProcess, NULL, szProcessName.data(), szProcessName.size());

        // GetModuleFileName returns a file path with back slashes. We need to convert that
        // to forward slashes which is the parameter of processName
        QString winProcName = QString::fromWCharArray(szProcessName.data())
                .replace(QStringLiteral("\\"), QStringLiteral("/"))
                .trimmed();

        CloseHandle(hProcess);

        if(processName == winProcName)
            return true;
        else
            qDebug () << "Found a process " << processName << " running with PID " << pid << " instead.";
    } else
        qDebug () << "Unable to open process " << pid;
    return false;

#endif

    // For mac and Linux
#ifdef Q_OS_UNIX
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
    qDebug () << "ps -p returns " << stdout;

    // processName contains an absolute path to the process according to the `Path` class
    //
    // ps -p can return /opt/piavpn/bin/vpn-client or ./vpn-client
    // or even simply 'vpn-client' if app was restarted from support tool
    //
    // On mac, the file names are mostly consistent but there's no
    // solid guarantee that this will always return the full path


    QFileInfo procInfo(processName), psInfo(stdout);
    // Ensure that the filenames are same for both the process and the output from `ps`
    // Also if the path is invalid, `fileName` will return an empty string. Check that this isn't empty.
    return (procInfo.fileName() == psInfo.fileName()) && !procInfo.fileName().isEmpty();
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
        processFound = findRunningProcess(*data, _executableName);

    // If no other instance is found, write the current PID as the primary running application.
    if(!processFound)
        *data = QCoreApplication::applicationPid();

    _pidShare.unlock();

    if(processFound)
      return *data;
    else
      return -1;
}
