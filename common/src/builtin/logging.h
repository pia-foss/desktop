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

#include "common.h"
#line HEADER_FILE("builtin/logging.h")

#ifndef BUILTIN_LOGGING_H
#define BUILTIN_LOGGING_H
#pragma once

#include "util.h"
#include <kapps_core/src/logger.h>

#include <QtDebug>
#include <QLoggingCategory>
#include <QString>
#include <iostream>
#include <QElapsedTimer>

// TODO - eliminate this alias
using CodeLocation = kapps::core::SourceLocation;

// Use the logger from kapps::core, we no longer use the Qt logger directly
#undef qFatal
#undef qCritical
#undef qWarning
#undef qInfo
#undef qDebug
#undef qCCritical
#undef qCWarning
#undef qCInfo
#undef qCDebug

#define qFatal KAPPS_CORE_FATAL
#define qError KAPPS_CORE_ERROR
#define qCritical KAPPS_CORE_ERROR   // qError was actually an alias to qCritical
#define qWarning KAPPS_CORE_WARNING
#define qInfo KAPPS_CORE_INFO
#define qDebug KAPPS_CORE_DEBUG
// Manual category macros (qCInfo, etc.) aren't defined, these are used very
// rarely, use KAPPS_CORE_INFO_CATEGORY(), etc.

#define CLASS_LOGGING_CATEGORY KAPPS_CORE_CLASS_CATEGORY
#define SCOPE_LOGGING_CATEGORY KAPPS_CORE_SCOPE_CATEGORY
#define FUNCTION_LOGGING_CATEGORY KAPPS_CORE_FUNCTION_CATEGORY
#define CURRENT_CATEGORY KAPPS_CORE_CURRENT_CATEGORY

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

    // Add a log redaction.  If the redact text occurs within a logging line, it
    // is replaced with "<<replace>>" (angle brackets added automatically).
    // This occurs before the trace is written to stderr or to log files, so the
    // sensitive value is never exposed.
    //
    // The redact text is matched exactly currently, but this could be expanded
    // to regex matching in the future if needed.  Both the redact and replace
    // texts should generally be ASCII only, due to encoding issues in
    // command-output redactions on Windows (shell commands on Windows normally
    // provide output in the current code page, but it's not 100% consistent).
    //
    // Adding the same redaction again updates the replacement text (the
    // duplicate redactions do not accumulate).
    //
    // Replacement texts should generally have some semantic meaning, so (for
    // example) different values can be differentiated, and so issues/warnings
    // could still be detected (for example, dedicated IP addresses and tokens
    // are replaced with <<DIP IP {region}>>, <<DIP token {{region}>>).
    //
    // There is no way to remove a redaction.  This is intentional, as in the
    // daemon it is difficult to be sure when a redaction could never occur
    // after the relevant value is removed (consider removing a dedicated IP
    // while still connected to that IP, etc.; OpenVPN might still trace the IP
    // at some point until the connection is completely torn down).
    static void addRedaction(const QString &redact, const QString &replace);
    // Redact a piece of text with the current redactions in Logger.  (Used when
    // generating a diagnostics file for a debug report.)
    static QString redactText(QString text);
    // Redact a piece of text in the default 8-bit encoding.
    static QByteArray redactText(QByteArray text);

    // Instantiate the singleton in the main thread after QCoreApplication has been created.
    explicit Logger(const Path &logFilePath);

    // Destroy the singleton in the same thread it was created.
    ~Logger();

    // Enable writing log output to file (always true in debug builds)
    bool logToFile() const;
    // List of filter rules to apply to log output
    QStringList filters() const;
    void wipeLogFile ();

    Q_SLOT void configure(bool logToFile, bool largeLogFiles, const QStringList& filters);
    Q_SIGNAL void configurationChanged(bool logToFile, const QStringList& filters);

public:
    // Used by the kapps::core::LogCallback implementation
    static void writeMsg(kapps::core::LogMessage msg);
private:
    // Qt log callback (for capturing tracing from Qt itself)
    static void loggingHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    // Log output implementation.  These are used as early as
    // Logger::initialize(), which is _before_ Logger has been created.  We have
    // to be able to capture early tracing before Logger is created - we can't
    // write it to the log files, but it still goes to stderr and can still
    // trigger fatal exits.
    static void fatalExit(LoggerPrivate *d);
    static void writeToConsoleNoLock(const kapps::core::StringSlice &data);
    static void writePrefixedMsg(LoggerPrivate *d, const std::string &logPrefix,
                                 std::string msg);
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
