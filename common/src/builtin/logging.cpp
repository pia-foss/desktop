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

#include "common.h"
#line SOURCE_FILE("builtin/logging.cpp")

#include "logging.h"
#include "error.h"
#include "path.h"
#include "util.h"
#include "version.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QThread>

#include <cstdlib>


#if defined(QT_DEBUG) && defined(Q_OS_WIN)
extern "C" Q_DECL_IMPORT void __stdcall OutputDebugStringW(const wchar_t *str);
#endif

// These globals are needed as they're used in Logger::initialize (before Logger::Logger)
namespace
{
    QMutex g_logMutex(QMutex::Recursive);
    QDateTime g_startTime;
    bool g_logToStdErr = false;
}

// The log limit in bytes
const qint64 logFileLimit = 4000000;

class LoggerPrivate
{
    CLASS_LOGGING_CATEGORY("logger")

    Q_DISABLE_COPY(LoggerPrivate)
    Q_DECLARE_PUBLIC(Logger)
    Logger * const q_ptr;

    LoggerPrivate(Logger* logger, const Path &logFilePath);

    QFile logFile;
    qint64 logSize;
    QStringList filters;
    QFileSystemWatcher watcher;
    Path logFilePath;

    static const QString defaultFilters;
    static const QString disabledFilters;

    // Use fileName != "" as the "should log to file" flag
    bool logToFile() const { return !logFile.fileName().isEmpty(); }

    // Read debug.txt and update config
    void readDebugFile(bool watchingDirectory = false);
    // Write current filters to debug.txt (will create file)
    void writeDebugFile();
    // Remove debug.txt file
    void removeDebugFile();
    // Attempt to open the log file for writing
    bool openLogFile(bool newSession = true);
    // Helper to write a pre-formatted chunk of lines to the log file
    void writeToLogFile(const QString& lines);

    // Wipe log file and backup log file if exists
    void wipeLogFile();
};

// This is the default "base" filterset applied when logging to disk is enabled.
//
// It is _not_ the same thing as the filterset applied in
// DaemonSettings::debugLogging (or debug.txt), or the default "debug" value for
// that setting in DaemonSettings::getDefaultDebugLogging.  That value is
// concatenated to this "base" value to determine the complete set of filters.
//
// So the "base" filter is the default filterset if you don't override anything
// else - if you manually create an empty debug.txt, this is what you get.
// Rules in DaemonSettings.debugLogging / debug.txt can override these rules.
// Of course, this doesn't matter much in practice since users virtually never
// apply custom rules.
const QString LoggerPrivate::defaultFilters { QStringLiteral("*.debug=true\nqt.*.debug=false\nqt.*.info=false\n") };
const QString LoggerPrivate::disabledFilters {
#ifdef QT_DEBUG
    QStringLiteral("*.debug=true\nqt.*.debug=false\nqt.*.info=false\n")
#else
    QStringLiteral("*.debug=false\n")
#endif
};

// See Singleton - CRTP template with static member in dynamic lib
template class COMMON_EXPORT Singleton<Logger>;

void Logger::initialize(bool logToStdErr)
{
    g_logMutex.lock();

    g_startTime = QDateTime::currentDateTimeUtc();
    g_logToStdErr = logToStdErr;
    qInstallMessageHandler(loggingHandler);

    g_logMutex.unlock();
}

void Logger::enableStdErr(bool logToStdErr)
{
    QMutexLocker lock{&g_logMutex};
    g_logToStdErr = logToStdErr;
}

Logger::Logger(const Path &logFilePath)
    : d_ptr(nullptr)
{
    // We have to null out d_ptr (above), then initialize it with a new
    // LoggerPrivate separately.  LoggerPrivate's constructor may try to log,
    // and if d_ptr is still uninitialized, we'd crash, since the logging
    // handler is already installed.
    d_ptr = new LoggerPrivate(this, logFilePath);

    Q_D(Logger);

    if (!QCoreApplication::instance())
        qCritical() << "Instantiated Logging singleton before QCoreApplication";

    d->readDebugFile();
}

Logger::~Logger()
{
    delete d_ptr;
}

bool Logger::logToFile() const
{
    Q_D(Logger);
    QMutexLocker lock(&g_logMutex);
    return d->logToFile();
}

QStringList Logger::filters() const
{
    Q_D(Logger);
    QMutexLocker lock(&g_logMutex);
    return d->filters;
}

void Logger::wipeLogFile()
{
    Q_D(Logger);
    d->wipeLogFile();
}

void Logger::configure(bool logToFile, const QStringList& filters)
{
    Q_D(Logger);
    bool changed = false, success = true, writeDebugFile = false, removeDebugFile = false;
    {
        QMutexLocker lock(&g_logMutex);
        if (logToFile && !d->logToFile())
        {
            if (d->openLogFile())
            {
                changed = true;
                writeDebugFile = true;
            }
            else
            {
                d->logFile.setFileName({});
                success = false;
            }
        }
        else if (!logToFile && d->logToFile())
        {
            d->logFile.close();
            d->logFile.remove();
            d->logFile.setFileName({});
            d->logSize = 0;
            changed = true;
        }
        if (filters != d->filters)
        {
            d->filters = filters;
            QLoggingCategory::setFilterRules((logToFile ? d->defaultFilters : d->disabledFilters) + filters.join('\n'));
            changed = true;
            if (logToFile)
                writeDebugFile = true;

            if(filters.isEmpty()) {
                removeDebugFile = true;
            }
        }
    }
    if (!success)
        qError() << "Unable to open log file for writing:" << d->logFilePath;
    if (removeDebugFile)
        d->removeDebugFile();
    else if (writeDebugFile)
        d->writeDebugFile();
    if (changed)
        emit configurationChanged(d->logToFile(), d->filters);
}


LoggerPrivate::LoggerPrivate(Logger* logger, const Path &logFilePath)
    : q_ptr(logger)
    , logSize(0)
    , logFilePath{logFilePath}
{
    QLoggingCategory::setFilterRules(disabledFilters + filters.join('\n'));

    QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, logger, [this]() { readDebugFile(true); });
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, logger, [this]() { readDebugFile(false); });
    // Occasionally, QFileSystemWatcher may trace here when calling addPath().
    // This depends on the platform and state of the files being watched.
    // This has historically caused crashes due to the logging handler already
    // being installed at this point if the Logger/LoggerPrivate initialization
    // doesn't guard for this.
    // Keep this trace around to make sure tracing at this point works fine.
    // This also depends on the compiler too, it seems to happen mostly on
    // Linux.
    qInfo() <<  "Initializing LoggerPrivate";

    QFile debugFile(Path::DebugFile);
    if (debugFile.exists())
        // Watch debug.txt itself for modification/deletion
        watcher.addPath(Path::DebugFile);
    else
        // Watch parent directory to see if debug.txt gets added
        watcher.addPath(Path::DebugFile.parent());
}

void LoggerPrivate::readDebugFile(bool watchingDirectory)
{
    Q_Q(Logger);

    QFile debugFile(Path::DebugFile);

    // Ignore irrelevant directory notifications
    if (watchingDirectory && (!watcher.files().isEmpty() || !debugFile.exists()))
        return;

    bool changed = false;
    if (debugFile.open(QFile::ReadOnly | QFile::Text))
    {
        QString filterString = QTextStream(&debugFile).readAll();
        QStringList filterLines = filterString.split('\n', QString::SkipEmptyParts);
        qInfo() << "Loaded debug.txt with filter rules:" << filterLines;
        debugFile.close();

        g_logMutex.lock();
        if (!logFile.isOpen() && openLogFile())
            changed = true;
        if (filterLines != filters)
        {
            filters = filterLines;
            QLoggingCategory::setFilterRules((logToFile() ? defaultFilters : disabledFilters) + filterString);
            changed = true;
        }
        g_logMutex.unlock();

        // Watch debug.txt when it exists
        watcher.removePath(Path::DebugFile.parent());
        watcher.addPath(Path::DebugFile);
    }
    else
    {
        qInfo() << "No debug.txt found; using default filter rules";

        g_logMutex.lock();
        if (logToFile())
        {
            logFile.close();
            // Clear out the file name, so logToFile() == false
            logFile.setFileName({});
            changed = true;
        }
        if (!filters.empty())
        {
            filters.clear();
            QLoggingCategory::setFilterRules(disabledFilters);
            changed = true;
        }
        g_logMutex.unlock();

        // Watch parent directory when debug.txt doesn't exist
        watcher.removePath(Path::DebugFile);
        watcher.addPath(Path::DebugFile.parent());
    }

    if (changed)
    {
        emit q->configurationChanged(logToFile(), filters);
    }
}

void LoggerPrivate::writeDebugFile()
{
    QFile debugFile(Path::DebugFile);
    if (debugFile.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
    {
        QTextStream(&debugFile) << filters.join('\n') << '\n';
    }
}

void LoggerPrivate::removeDebugFile()
{
    QFileInfo info(Path::DebugFile);
    if(info.exists() && info.isWritable()) {
        QFile debugFile(Path::DebugFile);
        qDebug () << "Removing debug.txt file";
        if(debugFile.remove())
            qDebug () << "Successfully removed debug.txt file";
        else
            qDebug () << "Couldn't remove debug.txt file";
    }
}

bool LoggerPrivate::openLogFile(bool newSession)
{
    logFile.setFileName(logFilePath);
    if (logFile.open(QFile::WriteOnly | QFile::Append | QFile::Text))
    {
        logSize = logFile.size();
        if (newSession)
        {
            if (logSize != 0)
            {
                {
                    QTextStream s(&logFile);
                    s << endl << endl << endl;
                }
                logSize = logFile.size();
            }
            qInfo() << "Starting log session (v" PIA_VERSION ")";
        }
        return true;
    }
    return false;
}

void LoggerPrivate::writeToLogFile(const QString& lines)
{
    if (logFile.isOpen())
    {
        QTextStream(&logFile) << lines;
        logFile.flush();
        logSize += lines.size();

        if(logSize > logFileLimit) {
            Path oldFilePath = logFilePath + oldFileSuffix;
            QFileInfo oldFileInfo(oldFilePath);

            if(oldFileInfo.exists()) {
                if(oldFileInfo.isWritable()) {
                    QFile::remove(oldFilePath);
                }
                else {
                    // If we cannot create a new backup file, or it cannot
                    // be deleted, clear the existing file.
                    logFile.resize(0);
                    logFile.seek(0);
                    logSize = 0;
                    return;
                }
            }
            // Copy the file to the old file
            // This also automatically closes the old file
            logFile.rename(oldFilePath);

            // Create and use a new log file
            openLogFile(false);
        }
    }
}

void LoggerPrivate::wipeLogFile()
{
    Path oldFilePath = logFilePath + oldFileSuffix;
    if(logToFile()) {
        qWarning () << "Tried to wipe logfile while logging still enabled.";
        return;
    }
    if(QFile::exists(logFilePath)) {
        QFile::remove(logFilePath);
    }
    if(QFile::exists(oldFilePath)) {
        QFile::remove(oldFilePath);
    }
}



static void renderMsgType(QTextStream& s, QtMsgType type)
{
    switch (type)
    {
    case QtFatalMsg:    s << "[fatal]"; break;
    case QtCriticalMsg: s << "[critical]"; break;
    case QtWarningMsg:  s << "[warning]"; break;
    case QtInfoMsg:     s << "[info]"; break;
    case QtDebugMsg:    s << "[debug]"; break;
    default:            s << "[??]"; break;
    }
}

static void renderLocation(QTextStream& s, const char* file, int line)
{
    if (file)
    {
        s << '[';
    #ifdef QT_DEBUG
        static QRegExp re("^(common|client|daemon)/src/");
        s << QDir(Path::SourceRootDir).relativeFilePath(QLatin1String(file)).replace(re, {});
    #else
        s << QLatin1String(file);
    #endif
        if (line)
            s << ':' << line;
        s << ']';
    }
}

static QString buildLogFilePrefix(QDateTime now, QtMsgType type,
                                  const QMessageLogContext &context)
{
    QString prefix;
    QTextStream s(&prefix, QIODevice::WriteOnly);

    auto tid = reinterpret_cast<quintptr>(QThread::currentThreadId());
    tid ^= tid >> 16;
#if QT_POINTER_SIZE > 4
    tid ^= tid >> 32;
#endif
    char tidHex[8];
    std::sprintf(tidHex, "%04x", (quint16)tid);

    s << now.toString("[yyyy-MM-dd hh:mm:ss.zzz]");

    s << '[' << tidHex << ']';
    if (context.category)
        s << '[' << QLatin1String(context.category) << ']';
    renderLocation(s, context.file, context.line);
    renderMsgType(s, type);

    return prefix;
}

static QString buildDebugOutputPrefix(QDateTime now, QtMsgType type,
                                      const QMessageLogContext &context)
{
    QString prefix;
    QTextStream s{&prefix, QIODevice::WriteOnly};

    // Log time since start of process instead of clock time
    qint64 time = g_startTime.msecsTo(now);
    int milliseconds = time % 1000; time /= 1000;
    int seconds = time % 60; time /= 60;
    int minutes = time % 60; time /= 60;
    int hours = time;

    if (hours)
        s << '[' << '+' << hours << ':' << right << qSetPadChar('0') << qSetFieldWidth(2) << minutes << qSetFieldWidth(0) << ':' << qSetFieldWidth(2) << seconds << qSetFieldWidth(0) << '.' << qSetFieldWidth(3) << milliseconds << reset << ']';
    else if (minutes)
        s << '[' << '+' << minutes << ':' << right << qSetPadChar('0') << qSetFieldWidth(2) << seconds << qSetFieldWidth(0) << '.' << qSetFieldWidth(3) << milliseconds << reset << ']';
    else
        s << '[' << '+' << seconds << '.' << right << qSetPadChar('0') << qSetFieldWidth(3) << milliseconds << reset << ']';

    if (context.category)
        s << '[' << QLatin1String(context.category) << ']';
    renderLocation(s, context.file, context.line);
    renderMsgType(s, type);

    return prefix;
}

void Logger::loggingHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Override with simpler endl; gets converted by text file handling anyway
    const char endl = '\n';

    QDateTime now{QDateTime::currentDateTimeUtc()};
    QString logPrefix{buildLogFilePrefix(now, type, context)};
    QString outputPrefix{buildDebugOutputPrefix(now, type, context)};

    QString logLines;
    QString outputLines;
    {
        QTextStream logStream(&logLines, QIODevice::WriteOnly);
        QTextStream outputStream(&outputLines, QIODevice::WriteOnly);
        for (const auto& line : msg.splitRef('\n'))
        {
            logStream << logPrefix << ' ' << line << endl;
            outputStream << outputPrefix << ' ' << line << endl;
        }
    }

    Logger* self = Logger::instance();
    LoggerPrivate* const d = self ? self->d_func() : nullptr;

    g_logMutex.lock();

#if defined(QT_DEBUG) && defined(Q_OS_WIN)
    if (isDebuggerPresent())
    {
        ::OutputDebugStringW(qUtf16Printable(outputLines));
    }
    else
#endif
    {
        if(g_logToStdErr)
            QTextStream(stderr, QIODevice::WriteOnly) << outputLines;
    }
    if (d)
    {
        d->writeToLogFile(logLines);
    }

    g_logMutex.unlock();

    // Failure to queue arguments is a programming error (and hard to debug),
    // assert to provide a way to debug it.
    Q_ASSERT(!msg.startsWith("QObject::connect: Cannot queue arguments of type"));

    if (type == QtFatalMsg)
    {
        // One last extra attempt to ensure file data is flushed
        if (d) d->logFile.close();
        // Abort - treat this as an unclean exit.  Also gives a chance to debug
        // in debug builds (this is how failed asserts are handled).
        std::abort();
    }
}

void QCustomMessageLogger::fatal(const Error& e)
{
    auto str = e.errorString().toUtf8();
    e.fatal("%*s", str.size(), str.data());
}
void QCustomMessageLogger::critical(const Error& e)
{
    e.critical().noquote() << e.errorString();
}
#if !defined(QT_NO_WARNING_OUTPUT)
void QCustomMessageLogger::warning(const Error& e)
{
    e.warning().noquote() << e.errorString();
}
#endif
#if !defined(QT_NO_INFO_OUTPUT)
void QCustomMessageLogger::info(const Error& e)
{
    e.info().noquote() << e.errorString();
}
#endif
#if !defined(QT_NO_DEBUG_OUTPUT)
void QCustomMessageLogger::debug(const Error& e)
{
    e.debug().noquote() << e.errorString();
}
#endif

const QString oldFileSuffix = QStringLiteral(".old");
