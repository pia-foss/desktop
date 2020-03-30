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
#line HEADER_FILE("builtin/logging.h")

#ifndef BUILTIN_LOGGING_H
#define BUILTIN_LOGGING_H
#pragma once

#include "util.h"

#include <QtDebug>
#include <QLoggingCategory>
#include <QString>
#include <iostream>
#include <QDebug>
#include <QElapsedTimer>

struct COMMON_EXPORT CodeLocation
{
    const QLoggingCategory* category;
    const char* file;
    int line;

    CodeLocation() : category(nullptr), file(nullptr), line(0) {}
    CodeLocation(const char* file, int line, const QLoggingCategory& category)
        : category(&category), file(file), line(line) {}

    const char* categoryName() const { return category ? category->categoryName() : ""; }

    template<class Error, typename... Args>
    inline Error createError(Args&... args)
    {
        return Error(std::move(*this), std::forward<Args>(args)...);
    }
};

#define THIS_LOCATION CodeLocation(LOG_FILE, LOG_LINE, currentLoggingCategory())
#define HERE THIS_LOCATION

inline std::ostream &operator<<(std::ostream &os, const CodeLocation &location)
{
    return os << "[" << location.categoryName() << "] "
        << location.file << ":" << location.line;
}

inline QDebug operator<<(QDebug d, const CodeLocation &location)
{
    QDebugStateSaver saver{d};
    return d.nospace() << "[" << location.categoryName() << "] "
        << location.file << ":" << location.line;
}

class COMMON_EXPORT LogEnableHelper
{
    const QLoggingCategory* const _cat;
public:
    explicit LogEnableHelper(const QLoggingCategory* cat) : _cat(cat) {}
    static LogEnableHelper test(const QLoggingCategory* cat, QtMsgType type) { return LogEnableHelper(cat && cat->isEnabled(type) ? cat : nullptr); }
    operator bool() const { return !_cat; } // evaluate to false if we have a category
    const char* name() const { return _cat->categoryName(); }
};

// Custom subclass to handle the qDebug(exception) syntax for logging
// exceptions with their original location context.
//
class COMMON_EXPORT QCustomMessageLogger : public QMessageLogger
{
public:
    using QMessageLogger::QMessageLogger;
    using QMessageLogger::fatal;
    using QMessageLogger::critical;
    using QMessageLogger::warning;
    using QMessageLogger::info;
    using QMessageLogger::debug;

    template<typename... Args>
    inline void Q_NORETURN fatal(const CodeLocation& l, Args&&... args) { QMessageLogger(l.file, l.line, nullptr, l.categoryName()).fatal(std::forward<Args>(args)...); }
    template<typename... Args>
    inline auto critical(const CodeLocation& l, Args&&... args) { return QMessageLogger(l.file, l.line, nullptr, l.categoryName()).critical(std::forward<Args>(args)...); }
    template<typename... Args>
    inline auto warning(const CodeLocation& l, Args&&... args) { return QMessageLogger(l.file, l.line, nullptr, l.categoryName()).warning(std::forward<Args>(args)...); }
    template<typename... Args>
    inline auto info(const CodeLocation& l, Args&&... args) { return QMessageLogger(l.file, l.line, nullptr, l.categoryName()).info(std::forward<Args>(args)...); }
    template<typename... Args>
    inline auto debug(const CodeLocation& l, Args&&... args) { return QMessageLogger(l.file, l.line, nullptr, l.categoryName()).debug(std::forward<Args>(args)...); }

    void Q_NORETURN fatal(const class Error& e);
    void critical(const class Error& e);
    void warning(const class Error& e)
#if defined(QT_NO_WARNING_OUTPUT)
    {}
#else
    ;
#endif
    void info(const class Error& e)
#if defined(QT_NO_INFO_OUTPUT)
    {}
#else
    ;
#endif
    void debug(const class Error& e)
#if defined(QT_NO_DEBUG_OUTPUT)
    {}
#else
    ;
#endif
};


#define LOG_FILE __FILE__
#define LOG_LINE __LINE__
#define LOG_FUNC nullptr

// Redefine the Qt debug logging macros to pick up a contextual category

#undef qFatal
#undef qCritical
#undef qWarning
#undef qInfo
#undef qDebug
#undef qCCritical
#undef qCWarning
#undef qCInfo
#undef qCDebug

#define LOG_IMPL(type, category) if (auto __cat = LogEnableHelper::test(&category(), type)) {} else QCustomMessageLogger(LOG_FILE, LOG_LINE, LOG_FUNC, __cat.name())

#define qFatal    QCustomMessageLogger(LOG_FILE, LOG_LINE, LOG_FUNC, currentLoggingCategory().categoryName()).fatal

#define qCCritical(category, ...) LOG_IMPL(QtCriticalMsg, category).critical(__VA_ARGS__)
#define qCWarning(category, ...)  LOG_IMPL(QtWarningMsg, category).warning(__VA_ARGS__)
#define qCInfo(category, ...)     LOG_IMPL(QtInfoMsg, category).info(__VA_ARGS__)
#define qCDebug(category, ...)    LOG_IMPL(QtDebugMsg, category).debug(__VA_ARGS__)

#define qCritical LOG_IMPL(QtCriticalMsg,currentLoggingCategory).critical
#define qWarning  LOG_IMPL(QtWarningMsg,currentLoggingCategory).warning
#define qInfo     LOG_IMPL(QtInfoMsg,currentLoggingCategory).info
#define qDebug    LOG_IMPL(QtDebugMsg,currentLoggingCategory).debug

#define qCError   qCCritical
#define qError    qCritical

#ifdef QT_DEBUG
#define qFatalIfRelease qCritical
#else
#define qFatalIfRelease qFatal
#endif

#if defined(QT_NO_DEBUG_OUTPUT)
#  undef qDebug
#  define qDebug QT_NO_QDEBUG_MACRO
#endif
#if defined(QT_NO_INFO_OUTPUT)
#  undef qInfo
#  define qInfo QT_NO_QDEBUG_MACRO
#endif
#if defined(QT_NO_WARNING_OUTPUT)
#  undef qWarning
#  define qWarning QT_NO_QDEBUG_MACRO
#endif

// Support legacy LOG(...) syntax as well
#define LOG_FATAL qFatal
#define LOG_CRITICAL qCritical
#define LOG_ERROR qError
#define LOG_WARNING qWarning
#define LOG_INFO qInfo
#define LOG_VERBOSE qInfo
#define LOG_DEBUG qDebug
#define LOG(type,...) LOG_##type(__VA_ARGS__)


// Last-resort match for 'currentLoggingCategory' that will return the default category
template<typename... Args> static inline const QLoggingCategory& currentLoggingCategory(Args&&...)
{
    return *QLoggingCategory::defaultCategory();
}

// Declare that any functions in the class should use its own logging category.
#define CLASS_LOGGING_CATEGORY(...) \
    protected: \
    static const QLoggingCategory& currentLoggingCategory() \
    { \
        static const QLoggingCategory category(__VA_ARGS__); \
        return category; \
    } \
    private:

/*
 * This macro doesn't work at the moment, because combined-compilation mode
 * eliminates file scopes
// Define that the current file should use its own logging category.
#define FILE_LOGGING_CATEGORY(...) \
    namespace { \
        static const QLoggingCategory& currentLoggingCategory() \
        { \
            static const QLoggingCategory category(__VA_ARGS__); \
            return category; \
        } \
    }
*/

// Define that the current scope should use its own logging category.
#define SCOPE_LOGGING_CATEGORY(...) \
    static const QLoggingCategory currentLoggingCategory(__VA_ARGS__)

#define FUNCTION_LOGGING_CATEGORY SCOPE_LOGGING_CATEGORY

#define CURRENT_CATEGORY (currentLoggingCategory())

class Path;
class LoggerPrivate;

class COMMON_EXPORT Logger;
// See Singleton - CRTP template with static member in dynamic lib
extern template class COMMON_EXPORT_TMPL_SPEC_DECL Singleton<Logger>;

// Singleton class for managing all logging.
// Note that although Singleton<> is not thread-safe, it's used by Logger, and
// logging may occur from multiple threads at runtime.  Logger must be
// constructed on the main thread before any other threads are created, and it
// must only be destroyed after all other threads exit.
class COMMON_EXPORT Logger : public QObject, public Singleton<Logger>
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(Logger)
    CLASS_LOGGING_CATEGORY("logger")

    LoggerPrivate * d_ptr;

public:
    // Call this function as early as possible in the entrypoint function.
    // logToStdErr indicates whether log lines should be sent to stderr
    // (regardless of whether file logging is enabled).
    //
    // On Windows, stderr logging only occurs if no debugger is attached.  If a
    // debugger is attached, log lines are sent to the debugger regardless of
    // logToStdErr.
    static void initialize(bool logToStdErr);
    static void enableStdErr(bool logToStdErr);

    // Instantiate the singleton in the main thread after QCoreApplication has been created.
    explicit Logger(const Path &logFilePath);

    // Destroy the singleton in the same thread it was created.
    ~Logger();

    // Enable writing log output to file (always true in debug builds)
    bool logToFile() const;
    // List of filter rules to apply to log output
    QStringList filters() const;
    void wipeLogFile ();

    Q_SLOT void configure(bool logToFile, const QStringList& filters);
    Q_SIGNAL void configurationChanged(bool logToFile, const QStringList& filters);

private:
    static void loggingHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
};

#define g_logger (Logger::instance())

// Replace daemon.log with daemon.log.old
extern COMMON_EXPORT const QString oldFileSuffix;

// TraceStopwatch traces how long a function took to execute; useful here when
// starting/stopping services, which is done synchronously but theoretically
// could take a long time.
//
// TraceStopwatch traces the elapsed time in its destructor (if it is still
// enabled).  It always traces the time from its construction, even if it was
// disabled/enabled - this just controls whether it will trace at all.
//
// The context message should typically be a string literal; it must outlive the
// TraceStopwatch.
class COMMON_EXPORT TraceStopwatch
{
    CLASS_LOGGING_CATEGORY("stopwatch");

public:
    // Create an enabled TraceStopwatch with a context message.
    // The context message can be 'nullptr' to create a disabled TraceStopwatch.
    explicit TraceStopwatch(const char *pMsg);
    ~TraceStopwatch();

private:
    TraceStopwatch(const TraceStopwatch &) = delete;
    TraceStopwatch &operator=(const TraceStopwatch &) = delete;

public:
    // Disable TraceStopwatch, it will not trace anything on destruction.
    void disable();
    // Enable TraceStopwatch (if pMsg is non-nullptr).  If it was already
    // enabled, the specified message replaces the old one.  If pMsg is nullptr,
    // the TraceStopwatch is disabled.
    void enable(const char *pMsg);

public:
    const char *_pMsg;
    QElapsedTimer _elapsed;
};

#endif // BUILTIN_LOGGING_H
