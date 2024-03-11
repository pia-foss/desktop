// Copyright (c) 2024 Private Internet Access, Inc.
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

#pragma once
#include <kapps_core/core.h>
#include "logger.h"
#include "util.h"

#ifndef KAPPS_CORE_OS_WINDOWS    // TODO - Windows backend for Process

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

namespace kapps { namespace core {

class KAPPS_CORE_EXPORT Executor
{
public:
    using StringVector = std::vector<std::string>;

public:
    // Create with the trace category.  Stdout/stderr will trace with categories
    // "<category>.stdout", "<category>.stderr".
    //
    // The category must be valid for the life of PosixExec.
    //
    // 'muteError' can be set to silence specific error text from the stderr
    // output of any executed command.  This is used for commands like
    // 'pfctl -f' that spew mostly-useless warnings to stderr all the time.  If
    // provided, the exact text given is deleted from stderr before it is
    // traced.
    //
    // Only one muteError can be specified currently.  To use different mute
    // errors for different commands, use different Executors.
    explicit Executor(const kapps::core::LogCategory &category,
                      std::string muteError = {});

private:
    std::string buildSubcategoryName(const kapps::core::LogCategory &category,
                                     const kapps::core::StringSlice &suffix);

private:
    // Implementation of bash()/cmd() - executes program with args, prints the
    // command, exit code, and stdout/stderr if anything unexpected is returned.
    // traceFunc is called to trace the command if tracing occurs; tracing is
    // different for bash() vs. cmd().
    int cmdImpl(const std::string &program, const StringVector &args,
                void(*traceFunc)(std::ostream &, const std::string&, const StringVector&),
                const StringVector &env, std::string *pOut,
                bool ignoreErrors);

public:
#if defined(KAPPS_CORE_OS_POSIX)
    // Execute a shell command with /bin/bash -c "cmd"
    int bash(const std::string &command, bool ignoreErrors = false);

    // Execute a shell command with /bin/bash -c "cmd" and return the stdout
    // If the command errors out, return an empty string (errors are also traced)
    std::string bashWithOutput(const std::string &command, bool ignoreErrors = false);
#endif

    // Execute a specific program with arguments
    int cmd(const std::string &program, const StringVector &args,
            bool ignoreErrors = false);

    // Execute a command with a specific environment.  If env is empty, the
    // existing environment is used.
    int cmdWithEnv(const std::string &program, const StringVector &args,
                  const StringVector &env, bool ignoreErrors = false);

#if defined(KAPPS_CORE_OS_POSIX)
    // Execute a command and return the output if successful.  (The output and
    // exit code are still traced.)  If the command fails, an empty string is
    // returned.
    //
    // Not provided on Windows because it involves a conversion from the local
    // 8-bit encoding to UTF-16; the local 8-bit encoding on Windows is
    // overridden to UTF-8 for logging output which may break this conversion.
    // (This is not usually needed on Windows anyway, usually APIs are
    // available.)
    std::string cmdWithOutput(const std::string &program, const StringVector &args);

    // Execute a command with cmdWithOutput() and match the output to a regular
    // expression.  If the output fails to match, it is traced.
/*     QRegularExpressionMatch cmdWithRegex(const std::string &program,
                                         const StringVector &args,
                                         const QRegularExpression &regex);
 */#endif

private:
    // Executor is thread-safe - the default executor could be used from any
    // thread.  Executing a process shouldn't require any state anyway.
    const kapps::core::LogCategory &_category;
    const std::string _stdoutCatName, _stderrCatName;
    const kapps::core::LogCategory _stdoutCategory, _stderrCategory;
    const std::string _muteError;
};

namespace Exec
{
#if defined(KAPPS_CORE_OS_POSIX)
    // Execute a shell command with /bin/bash -c "cmd", using the default logging
    // category
    int KAPPS_CORE_EXPORT bash(const std::string &command, bool ignoreErrors = false);

    // Execute a shell command with /bin/bash -c "cmd", using the default logging
    // category. Also return the stdout of the command (or an empty string on error)
    std::string KAPPS_CORE_EXPORT bashWithOutput(const std::string &command, bool ignoreErrors = false);
#endif

    // Execute a process with arguments, tracing exit code/stdout/stderr, using the
    // default logging category
    int KAPPS_CORE_EXPORT cmd(const std::string &program, const Executor::StringVector &args,
                          bool ignoreErrors = false);

    // Execute a process with arguments and return the stdout (or empty string on error)
    std::string KAPPS_CORE_EXPORT cmdWithOutput(const std::string &program, const Executor::StringVector &args);


    int KAPPS_CORE_EXPORT cmdWithEnv(const std::string &program, const Executor::StringVector &args,
                                 const Executor::StringVector &env,
                                 bool ignoreErrors = false);
}
}}

#endif
