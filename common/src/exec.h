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
#line HEADER_FILE("exec.h")

#ifndef EXEC_H
#define EXEC_H

#include <QProcessEnvironment>
#include <QRegularExpression>

// Execute a shell command or process, and trace the exit code/stdout/stderr if
// the results are unexpected.
//
// Executor stores specific logging categories to differentiate executions in
// different contexts.  To trace with the default categories, use Exec::bash()
// or Exec::cmd().
//
// On Windows, bash() is not available (only cmd()), and cmd() quotes arguments
// in the same way as QProcess::start().  This works for most programs, but not
// cmd.exe or batch scripts.  (QProcess provides setNativeArguments() for
// Windows, but Executor doesn't provide this currently.)
class COMMON_EXPORT Executor
{
public:
    // Create with the trace category.  Stdout/stderr will trace with categories
    // "<category>.stdout", "<category>.stderr".
    // The category must be valid for the life of PosixExec.
    explicit Executor(const QLoggingCategory &category);

private:
    QByteArray buildSubcategoryName(const QLoggingCategory &category,
                                    const QLatin1String &suffix);

private:
    // Implementation of bash()/cmd() - executes program with args, prints the
    // command, exit code, and stdout/stderr if anything unexpected is returned.
    // traceFunc is called to trace the command if tracing occurs; tracing is
    // different for bash() vs. cmd().
    int cmdImpl(const QString &program, const QStringList &args,
                void(*traceFunc)(QDebug &, const QString&, const QStringList&),
                const QProcessEnvironment &env, QString *pOut,
                bool ignoreErrors);

    void cmdDetached(const QString &program, const QStringList &args);

public:
#if defined(Q_OS_UNIX)
    // Execute a shell command with /bin/bash -c "cmd"
    int bash(const QString &command, bool ignoreErrors = false);
    void bashDetached(const QString &command);

    // Execute a shell command with /bin/bash -c "cmd" and return the stdout
    // If the command errors out, return an empty string (errors are also traced)
    QString bashWithOutput(const QString &command, bool ignoreErrors = false);
#endif

    // Execute a specific program with arguments
    int cmd(const QString &program, const QStringList &args,
            bool ignoreErrors = false);

    // Execute a command with a specific environment.  If env is empty, the
    // existing environment is used.
    int cmdWithEnv(const QString &program, const QStringList &args,
                  const QProcessEnvironment &env, bool ignoreErrors = false);

#if defined(Q_OS_UNIX)
    // Execute a command and return the output if successful.  (The output and
    // exit code are still traced.)  If the command fails, an empty string is
    // returned.
    //
    // Not provided on Windows because it involves a conversion from the local
    // 8-bit encoding to UTF-16; the local 8-bit encoding on Windows is
    // overridden to UTF-8 for logging output which may break this conversion.
    // (This is not usually needed on Windows anyway, usually APIs are
    // available.)
    QString cmdWithOutput(const QString &program, const QStringList &args);

    // Execute a command with cmdWithOutput() and match the output to a regular
    // expression.  If the output fails to match, it is traced.
    QRegularExpressionMatch cmdWithRegex(const QString &program,
                                         const QStringList &args,
                                         const QRegularExpression &regex);
#endif

private:
    const QLoggingCategory &_category;
    QByteArray _stdoutCatName, _stderrCatName;
    QLoggingCategory _stdoutCategory, _stderrCategory;
};

namespace Exec
{
#if defined(Q_OS_UNIX)
    // Execute a shell command with /bin/bash -c "cmd", using the default logging
    // category
    int COMMON_EXPORT bash(const QString &command, bool ignoreErrors = false);
    void COMMON_EXPORT bashDetached(const QString &command);

    // Execute a shell command with /bin/bash -c "cmd", using the default logging
    // category. Also return the stdout of the command (or an empty string on error)
    QString COMMON_EXPORT bashWithOutput(const QString &command, bool ignoreErrors = false);
#endif

    // Execute a process with arguments, tracing exit code/stdout/stderr, using the
    // default logging category
    int COMMON_EXPORT cmd(const QString &program, const QStringList &args,
                          bool ignoreErrors = false);

    int COMMON_EXPORT cmdWithEnv(const QString &program, const QStringList &args,
                                 const QProcessEnvironment &env,
                                 bool ignoreErrors = false);

#if defined(Q_OS_UNIX)
    // Execute a process with arguments and return the stdout (or empty string on error)
    QString COMMON_EXPORT cmdWithOutput(const QString &program, const QStringList &args);

    QRegularExpressionMatch COMMON_EXPORT cmdWithRegex(const QString &program,
                                                       const QStringList &args,
                                                       const QRegularExpression &regex);
#endif
}

#endif
