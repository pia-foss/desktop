// Copyright (c) 2023 Private Internet Access, Inc.
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

#if defined(Q_OS_LINUX) && PIA_CLIENT
#include "../exec.h"
#endif

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
#include <unordered_map>
#include <cstring>

#if defined(QT_DEBUG) && defined(Q_OS_WIN)
extern "C" Q_DECL_IMPORT void __stdcall OutputDebugStringW(const wchar_t *str);
#endif

// These globals are needed as they're used in Logger::initialize (before Logger::Logger)
namespace
{
    QMutex g_logMutex(QMutex::Recursive);
    bool g_logToStdErr = false;

    // kapps::core::LogCallback implementation, forwards to Logger::writeMessage()
    // 'final' here silences a warning from clang that a nonvirtual destructor
    // of LoggerCallback is called on a polymorphic class, which is fine here
    // since we didn't derive from LoggerCallback.
    class LoggerCallback final : public kapps::core::LogCallback
    {
    public:
        virtual void write(kapps::core::LogMessage msg) override
        {
            Logger::writeMsg(std::move(msg));
        }
    };
    // Log redactions - maps redact strings to replacements (which now include
    // the angle brackets).  All redactions are applied sequentially when
    // redacting text, but they're stored in a map so that adding the same
    // redaction again doesn't accumulate.
    std::unordered_map<std::string, std::string> g_redactions;

    QString redactTextNoLock(QString text)
    {
        for(const auto &redaction : g_redactions)
        {
            text.replace(QString::fromStdString(redaction.first),
                         QString::fromStdString(redaction.second));
        }
        return text;
    }

    QByteArray redactTextNoLock(QByteArray text)
    {
        for(const auto &redaction : g_redactions)
        {
            text.replace(redaction.first.c_str(), redaction.first.size(),
                         redaction.second.c_str(), redaction.second.size());
        }
        return text;
    }

    std::string redactTextNoLock(std::string text)
    {
        for(const auto &redaction : g_redactions)
        {
            while(true)
            {
                auto pos = text.find(redaction.first);
                if(pos == std::string::npos)
                    break;
                text.replace(pos, redaction.first.size(), redaction.second);
            }
        }
        return text;
    }
}

// The log limit in bytes
const qint64 standardLogFileLimit = 4000000;
const qint64 largeLogFileLimit = 40000000;

class LoggerPrivate
{
    CLASS_LOGGING_CATEGORY("logger")

    Q_DISABLE_COPY(LoggerPrivate)
    Q_DECLARE_PUBLIC(Logger)
    Logger * const q_ptr;

    LoggerPrivate(Logger* logger, const Path &logFilePath);

    QFile logFile;
    qint64 logSize;
    qint64 logFileLimit = standardLogFileLimit;
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
    void writeToLogFile(const kapps::core::StringSlice &data);

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

    g_logToStdErr = logToStdErr;
    qInstallMessageHandler(loggingHandler);

    g_logMutex.unlock();

    kapps::core::log::enableLogging(true);
    kapps::core::log::init(std::make_shared<LoggerCallback>());
}

void Logger::enableStdErr(bool logToStdErr)
{
    QMutexLocker lock{&g_logMutex};
    g_logToStdErr = logToStdErr;
}

void Logger::addRedaction(const QString &redact, const QString &replace)
{
    QMutexLocker lock{&g_logMutex};
    g_redactions[redact.toStdString()] = QStringLiteral("<<%1>>").arg(replace).toStdString();
}

QString Logger::redactText(QString text)
{
    QMutexLocker lock{&g_logMutex};
    return redactTextNoLock(std::move(text));
}

QByteArray Logger::redactText(QByteArray text)
{
    QMutexLocker lock{&g_logMutex};
    return redactTextNoLock(std::move(text));
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

    // QFileSystemWatcher requires a QCoreApplication to exist, so Logger can't
    // be initialized until QCoreApplication is up.
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

void Logger::configure(bool logToFile, bool largeLogFiles, const QStringList& filters)
{
    Q_D(Logger);
    bool changed = false, success = true, writeDebugFile = false, removeDebugFile = false;
    {
        d->logFileLimit = largeLogFiles ? largeLogFileLimit : standardLogFileLimit;
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
    logFilePath.mkparent();
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
            qInfo().nospace() << "Starting log session (v"
                << Version::semanticVersion() << ")";
        }
        return true;
    }
    return false;
}

void LoggerPrivate::writeToLogFile(const kapps::core::StringSlice &data)
{
    if (logFile.isOpen())
    {
        logFile.write(data.data(), data.size());
        logFile.flush();
        logSize += data.size();

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

namespace
{
    void renderMsgType(std::ostream &s, QtMsgType type)
    {
        switch (type)
        {
            case QtFatalMsg:    s << "[fatal]"; break;
            case QtCriticalMsg: s << "[error]"; break;
            case QtWarningMsg:  s << "[warning]"; break;
            case QtInfoMsg:     s << "[info]"; break;
            case QtDebugMsg:    s << "[debug]"; break;
            default:            s << "[??]"; break;
        }
    }

    void renderMsgType(std::ostream &s, kapps::core::LogMessage::Level type)
    {
        switch (type)
        {
            case kapps::core::LogMessage::Level::Fatal:   s << "[fatal]"; break;
            case kapps::core::LogMessage::Level::Error:   s << "[error]"; break;
            case kapps::core::LogMessage::Level::Warning: s << "[warning]"; break;
            case kapps::core::LogMessage::Level::Info:    s << "[info]"; break;
            case kapps::core::LogMessage::Level::Debug:   s << "[debug]"; break;
            default:                                     s << "[??]"; break;
        }
    }

    void renderTimeThread(std::ostream &os)
    {
        auto tid = reinterpret_cast<quintptr>(QThread::currentThreadId());
        tid ^= tid >> 16;
    #if QT_POINTER_SIZE > 4
        tid ^= tid >> 32;
    #endif
        char tidHex[8];
        std::sprintf(tidHex, "%04x", (quint16)tid);

        // TODO - Should render directly to UTF-8
        QDateTime now{QDateTime::currentDateTimeUtc()};
        os << now.toString("[yyyy-MM-dd hh:mm:ss.zzz]").toStdString();

        os << '[' << tidHex << ']';
    }

    std::string buildLogFilePrefix(kapps::core::LogMessage::Level type,
                                   const kapps::core::SourceLocation &loc,
                                   const kapps::core::LogCategory &cat)
    {
        std::stringstream s{std::ios_base::out};

        renderTimeThread(s);
        s << '[' << cat << "][" << (loc.file() ? loc.file() : "??")
            << ':' << loc.line() << ']';
        renderMsgType(s, type);
        s << ' ';

        // TODO - Can't get content of stringstream without unnecessary copy in
        // C++14
        return s.str();
    }

    std::string buildLogFilePrefix(QtMsgType type, const QMessageLogContext &context)
    {
        std::stringstream s{std::ios_base::out};

        renderTimeThread(s);
        s << '[' << QLatin1String{context.category ? context.category : "??"} << ']';
        s << '[' << (context.file ? context.file : "??") << ':' << context.line << ']';
        renderMsgType(s, type);
        s << ' ';

        return s.str();
    }
}

void Logger::loggingHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
#if defined(Q_OS_WIN)
    // We force ANGLE in client/src/main.cpp to avoid Qt loading OpenGL, which
    // crashes on certain buggy drivers.  However, this means that it logs
    // errors relating to the failure to load OpenGL.  These can safely be
    // ignored since we don't use OpenGL.
    if(msg.startsWith("Failed to load libEGL") ||
        msg.startsWith("QWindowsEGLStaticContext::create: Failed to load and resolve libEGL functions"))
    {
        return; // Silently ignore this trace
    }
#endif

    Logger* self = Logger::instance();
    LoggerPrivate *d = self ? self->d_func() : nullptr;

    std::string logPrefix{buildLogFilePrefix(type, context)};

    writePrefixedMsg(d, logPrefix, msg.toStdString());

    // Failure to queue arguments is a programming error (and hard to debug),
    // assert to provide a way to debug it.
    Q_ASSERT(!msg.startsWith("QObject::connect: Cannot queue arguments of type"));

    if (type == QtFatalMsg)
    {
#if defined(Q_OS_LINUX) && PIA_CLIENT
        if(msg.contains(QStringLiteral("Failed to create OpenGL context")))
        {
#ifdef PIA_CRASH_REPORTING
            stopCrashReporting();
#endif // PIA_CRASH_REPORTING
            Exec::bashDetached(Path::ExecutableDir / "error-notice.sh");
        }
#endif // defined(Q_OS_LINUX) && PIA_CLIENT

        fatalExit(d);
    }
}

void Logger::writeMsg(kapps::core::LogMessage msg)
{
    Logger* self = Logger::instance();
    LoggerPrivate *d = self ? self->d_func() : nullptr;

    std::string logPrefix{buildLogFilePrefix(msg.level(), msg.loc(), msg.category())};
    writePrefixedMsg(d, logPrefix, std::move(msg).message());

    if(msg.level() == kapps::core::LogMessage::Level::Fatal)
        fatalExit(d);
}

void Logger::fatalExit(LoggerPrivate *d)
{
    // One last extra attempt to ensure file data is flushed
    if (d)
        d->logFile.close();

    // Abort - treat this as an unclean exit.  Also gives a chance to debug
    // in debug builds (this is how failed asserts are handled).
    std::abort();
}

void Logger::writeToConsoleNoLock(const kapps::core::StringSlice &data)
{
#if defined(QT_DEBUG) && defined(Q_OS_WIN)
    if (isDebuggerPresent())
    {
        // Windows needs UTF-16 :-/
        QString dataUtf16{QString::fromUtf8(data.data(), data.size())};
        ::OutputDebugStringW(qUtf16Printable(dataUtf16));
    }
    else
#endif
    {
        if(g_logToStdErr)
            std::cerr << data;
    }
}

void Logger::writePrefixedMsg(LoggerPrivate *d, const std::string &logPrefix, std::string msg)
{
    g_logMutex.lock();

    std::string redacted = redactTextNoLock(std::move(msg));

    // Slice out each line of the message and log it with the prefix
    std::size_t lineEnd = 0;
    while(lineEnd < redacted.size())
    {
        std::size_t lineStart = lineEnd;
        lineEnd = redacted.find('\n', lineEnd);
        if(lineEnd == std::string::npos)
            lineEnd = redacted.size();   // Consume rest of the string, lineEnd now points to '\0'
        else
            ++lineEnd;  // Include the line break in the output

        kapps::core::StringSlice line{redacted.c_str() + lineStart,
                                     redacted.c_str() + lineEnd};

        writeToConsoleNoLock(logPrefix);
        writeToConsoleNoLock(line);
        if(d)
        {
            d->writeToLogFile(logPrefix);
            d->writeToLogFile(line);
        }
    }

    // Terminate the last line
    writeToConsoleNoLock("\n");
    if(d)
        d->writeToLogFile("\n");

    g_logMutex.unlock();
}

const QString oldFileSuffix = QStringLiteral(".old");

TraceStopwatch::TraceStopwatch(const char *pMsg)
    : _pMsg{pMsg}
{
    _elapsed.start();
}

TraceStopwatch::~TraceStopwatch()
{
    if(_pMsg)
    {
        qInfo() << _pMsg << "-" << traceMsec(_elapsed.elapsed());
    }
}

void TraceStopwatch::disable()
{
    enable(nullptr);
}

void TraceStopwatch::enable(const char *pMsg)
{
    _pMsg = pMsg;
}
