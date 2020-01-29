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

#include "common.h"
#line SOURCE_FILE("builtin/util.cpp")

#include "util.h"
#include "error.h"
#include "path.h"

#ifdef PIA_CRASH_REPORTING
#if defined(Q_OS_MACOS)
#include "client/mac/handler/exception_handler.h"
#elif defined(Q_OS_WIN)
#include "client/windows/handler/exception_handler.h"
#elif defined(Q_OS_LINUX)
#include "client/linux/handler/exception_handler.h"
#endif
#endif

#include <QFile>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QTextCodec>
#include <QElapsedTimer>

#ifdef QT_DEBUG
# if defined(Q_OS_WIN)
    extern "C" Q_DECL_IMPORT int __stdcall IsDebuggerPresent();
# elif defined(Q_OS_MACOS)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/sysctl.h>
# endif
#endif

std::tuple<int, QByteArray, QByteArray> shellExecute(const QString &command, bool showDebug)
{
    QProcess p;
    p.start(QStringLiteral("/bin/bash"), { QStringLiteral("-c"), command });
    p.closeWriteChannel();
    auto exitCode = waitForExitCode(p);
    QByteArray out = p.readAllStandardOutput().trimmed();
    QByteArray err = p.readAllStandardError().trimmed();
    auto result = std::make_tuple(exitCode, out, err);

    if(exitCode != 0)
    {
        qWarning().noquote() << "Command:" << command << command << "failed with exit code:" << exitCode << "\n"
            << "STDERR:" << err << "\n"
            << "STDOUT:" << out;
    }
    else if(showDebug)
    {
        qInfo() << "Executed" << command << "\n"
            << "STDERR:" << err << "\n"
            << "STDOUT:" << out;
    }

    return result;
}

QString traceMsec(qint64 time)
{
    Q_ASSERT(time >= 0);
    qint64 minutes = time / 60000;
    qint64 msec = time % 60000;
    double secDbl = msec / 1000.0;

    if(minutes > 0)
        return QStringLiteral("%1 min %2 sec").arg(minutes).arg(secDbl);
    return QStringLiteral("%1 sec").arg(secDbl);
}

static bool checkIfDebuggerPresent()
{
#ifdef QT_DEBUG
#if defined(Q_OS_WIN)
    return ::IsDebuggerPresent() != 0;
#elif defined(Q_OS_MACOS)
    struct kinfo_proc info;
    memset(&info, 0, sizeof(info));
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    size_t size = sizeof(info);
    sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#endif
#endif
    return false;
}

bool isDebuggerPresent()
{
    static bool present = checkIfDebuggerPresent();
    return present;
}


int waitForExitCode(QProcess& process)
{
    if (!process.waitForFinished() || process.error() == QProcess::FailedToStart)
        return -2;
    else if (process.exitStatus() != QProcess::NormalExit)
        return -1;
    else
        return process.exitCode();
}


void throwIfEnumCastFailed(bool result, const char* name)
{
    if (!result)
        throw Error(HERE, Error::InvalidEnumValue, { QString(name) });
}


void startSupportTool (const QString &mode, const QString &diagFile)
{
#ifdef PIA_CLIENT
    QProcess crashReportProcess;
    QStringList args;

    args << "--mode" << mode;
    args << "--log" << Path::ClientLogFile;
    args << "--log" << Path::DaemonLogFile;
    args << "--log" << Path::CliLogFile;
    args << "--log" << Path::ConfigLogFile;
    args << "--log" << Path::UpdownLogFile;
    if(!diagFile.isEmpty())
        args << "--file" << diagFile;
    args << "--client-crashes" << Path::ClientDataDir / "crashes";
    args << "--daemon-crashes" << Path::DaemonDataDir / "crashes";
    args << "--client-settings" << Path::ClientSettingsDir / "clientsettings.json";

    qDebug () << "Starting support tool at " << Path::SupportToolExecutable;
    qDebug () << args;
    crashReportProcess.setProgram(Path::SupportToolExecutable);
    crashReportProcess.setArguments(args);
    crashReportProcess.startDetached();
#endif
}

void setUtf8LocaleCodec()
{
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
}

#ifdef PIA_CRASH_REPORTING


// Different implementations of `DumpCallback for each platform.
// None of these attempt to write out diagnostics.
// - If the client is crashing, it is probably not a good idea to try to use all
//   the RPC code - it might not be in a sane state due to the crash (or might
//   even have caused it).
// - If the daemon is crashing, we might safely be able to write diagnostics,
//   but there's currently no way to communicate the result to the client, which
//   would actually start the support tool.
#if defined(Q_OS_MAC)
bool DumpCallback(const char *dump_dir,
                  const char *minidump_id,
                  void *context, bool succeeded) {
    Q_UNUSED(context);

    QString path = QString::fromUtf8(dump_dir) + QLatin1String("/") + QString::fromUtf8(minidump_id) + ".dmp";
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", qPrintable(path));


    if(succeeded) {
#ifdef PIA_CLIENT
        startSupportTool(QStringLiteral("crash"), {});
#endif

#if defined(PIA_DAEMON)
        QFile::setPermissions(path, QFileDevice::ReadUser|QFileDevice::ReadGroup);
#endif
    }
    return succeeded;
}
#elif defined(Q_OS_WIN)
bool DumpCallback(const wchar_t* dump_dir,
                                    const wchar_t* minidump_id,
                                    void* context,
                                    EXCEPTION_POINTERS* exinfo,
                                    MDRawAssertionInfo* assertion,
                                    bool succeeded)
{
    Q_UNUSED(assertion);
    Q_UNUSED(exinfo);

    QString path = QString::fromWCharArray(dump_dir) + QLatin1String("/") + QString::fromWCharArray(minidump_id) + ".dmp";
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", qPrintable(path));

    if(succeeded) {
        startSupportTool("crash", {});
    }

    return succeeded;
}
#elif defined(Q_OS_LINUX)
bool DumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                                    void* context,
                  bool succeeded) {
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", descriptor.path());

    if(succeeded) {
#if defined(PIA_DAEMON)
        QFile::setPermissions(descriptor.path(), QFileDevice::ReadUser|QFileDevice::ReadGroup);
#endif
        startSupportTool("crash", {});
    }

    return succeeded;
}
#endif

// Initialize crash reporting. Please ensure Path is initialized before this
void initCrashReporting()
{
    if(isDebuggerPresent())
    {
        qInfo() << "Being debugged - not handling crashes";
        return;
    }

    qInfo() << "Initializing crash handler";

    Path::CrashReportDir.mkpath();
    // Create a pointer to exception handler. Since only one object
    // is created, and is long running, no need to store reference for clearing/usage later
#if defined(Q_OS_MAC)
    new google_breakpad::ExceptionHandler(QString(Path::CrashReportDir).toStdString(),
                                          /*FilterCallback*/ 0,
                                          DumpCallback, /*context*/ 0, true, NULL);
#elif defined(Q_OS_WIN)
    new google_breakpad::ExceptionHandler(QString(Path::CrashReportDir).toStdWString(), /*FilterCallback*/ 0,
                                          DumpCallback, /*context*/ 0,
                                          google_breakpad::ExceptionHandler::HANDLER_ALL);
#elif defined(Q_OS_LINUX)
    new google_breakpad::ExceptionHandler(google_breakpad::MinidumpDescriptor(QString(Path::CrashReportDir).toStdString()),
                                                            /*FilterCallback*/ 0,
                                                            DumpCallback,
                                                            /*context*/ 0,
                                                            true,
                                                            -1);
#endif
}

void monitorDaemonDumps()
{
    // Ideally, `monitorDaemonDumps` is only called once, so we can create this object
    // without worrying about destroying it. Since it needs to run for the duration of the client
    auto daemonCrashWatcher = new QFileSystemWatcher();

    // Currently we are hard-coding the daemon crash dir
    const auto &daemonCrashDir = Path::DaemonDataDir / QStringLiteral("crashes");
    if(!daemonCrashWatcher->addPath(daemonCrashDir))
    {
        qWarning() << "Failed to watch daemon crash directory" << daemonCrashDir;
        // TODO - We need to watch for the directory to be created - if we get
        // here before the daemon creates the directory for the first time, then
        // we won't be able to monitor for daemon crashes.
    }
    QObject::connect(daemonCrashWatcher, &QFileSystemWatcher::directoryChanged, [daemonCrashDir](const QString &paths) {
        // There are chances that the directory might have changed for reasons other than
        // a legitimate daemon crash. For this reason, we can iterate through all the files
        // in this folder and only start support tool when a .dmp file is found
        // that was created in the last 30 seconds
        QDirIterator di(daemonCrashDir, QStringList() << "*.dmp", QDir::Files);

        while (di.hasNext()) {
            QFileInfo fileInfo(di.next());
            QDateTime createdTime = fileInfo.birthTime();
            // On linux, sometime the birthTime is marked as invalid. Since the scope of this fix
            // is predominantly windows, assume that any invalid time file is a valid crash.
            if(createdTime.secsTo(QDateTime::currentDateTime()) < 30 || (!createdTime.isValid())) {
                qDebug () << "Found a daemon crash dmp with timestamp: " << createdTime;

                // Maybe we could try to wait for the daemon to come back up and ask it
                // to write diagnostics, but this would be relatively complex to handle
                // all the possibilities of it failing and ensure that we still report
                // the problem.
                startSupportTool(QStringLiteral("crash"), {});
                break;
            }
        }
    });
}



#ifdef Q_OS_LINUX
void stopCrashReporting()
{
    // This is used in the Linux/X11 atexit() handler.  Due to the Xlib issues
    // described there, the process may crash as we're shutting down, and
    // Breakpad has been observed to deadlock in this state.
    // This is the only thing Breakpad checks before locking its mutex,
    // which seems to cause the deadlock.
    google_breakpad::SetFirstChanceExceptionHandler([](int, void*, void*) {return true;});
}
#endif

#endif

qint64 getMonotonicTime() {
    // Although QElapsedTimer does not guarantee in general that it uses a
    // system-wide epoch, it does on all platforms we support (it's the time
    // since boot).
    QElapsedTimer timer;
    timer.start();
    return timer.msecsSinceReference();
}

void testCrash()
{
    auto *crash = reinterpret_cast<volatile int *>(0);
    *crash = 0;
}
