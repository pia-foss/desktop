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
#line SOURCE_FILE("exec.cpp")

#include "exec.h"
#include <QProcess>

namespace
{
    // Tracer for cmd() and friends
    const auto traceCmd = [](std::ostream &os, const QString &program, const QStringList &args)
    {
        os << program;
        for(const auto &arg : args)
            os << ' ' << arg;
    };

#ifdef Q_OS_UNIX
    // Tracer for bash() and friends
    const auto traceShellCmd = [](std::ostream &os, const QString&, const QStringList &args)
    {
        // Just trace "$ <command>".  Don't quote the command again.
        // (program is "bash" and args[0] is "-c", don't need to see those)
        os << "$ " << qUtf8Printable(args[1]);
    };
#endif
}

Executor::Executor(const kapps::core::LogCategory &category)
    : _category{category},
      _stdoutCatName{buildSubcategoryName(category, ".stdout")},
      _stderrCatName{buildSubcategoryName(category, ".stderr")},
      _stdoutCategory{*category.module(), _stdoutCatName},
      _stderrCategory{*category.module(), _stderrCatName}
{
}

std::string Executor::buildSubcategoryName(const kapps::core::LogCategory &category,
                                           const kapps::core::StringSlice &suffix)
{
    std::string subcatName;
    const auto &catName{category.name()};
    subcatName.reserve(catName.size() + suffix.size());
    subcatName.append(catName.data(), catName.size());
    subcatName.append(suffix.data(), suffix.size());
    return subcatName;
}

// bash - $ command
// cmd - program args

// This used to be necessary when tracing was based on QDebug since we had to
// use a postfix expression to call the trace functor.  It could be reworked
// for the kapps::core logger.
struct CmdTrace
{
    using FuncT = void(*)(std::ostream&, const QString&, const QStringList&);
    const QString &program;
    const QStringList &args;
    FuncT pFunc;
};

std::ostream &operator<<(std::ostream &os, const CmdTrace &trace)
{
    Q_ASSERT(trace.pFunc);    // Ensured by caller
    trace.pFunc(os, trace.program, trace.args);
    return os;
}

int Executor::cmdImpl(const QString &program, const QStringList &args,
                      void(*traceFunc)(std::ostream &, const QString&, const QStringList&),
                      const QProcessEnvironment &env, QString *pOut,
                      bool ignoreErrors)
{
    Q_ASSERT(traceFunc);    // Ensured by caller

    QProcess p;

    // Set the process environment (if provided)
    if(!env.isEmpty()) p.setProcessEnvironment(env);
    p.start(program, args, QProcess::ReadOnly);
    p.closeWriteChannel();
    int exitCode = waitForExitCode(p);
    auto out = p.readAllStandardOutput().trimmed();
    auto err = p.readAllStandardError().trimmed();
    if(pOut)
    {
#if defined(Q_OS_WIN)
        // This is not likely to be reliable on Windows, output is usually in
        // the current system code page, which we have overridden with UTF-8
        // (see setUtf8LocaleCodec()).  It might work for en-US but it is likely
        // to break for other locales (in particular, interface names on Windows
        // can contain non-ASCII characters).
        Q_ASSERT(false);
#endif
        *pOut = QString::fromUtf8(out);
    }

    if ((exitCode != 0 || !err.isEmpty()) && !ignoreErrors)
    {
        KAPPS_CORE_WARNING_CATEGORY(_category).nospace() << '(' << exitCode
            << ')' << CmdTrace{program, args, traceFunc};
    }
    if (!out.isEmpty())
    {
        KAPPS_CORE_INFO_CATEGORY(_stdoutCategory) << out.data();
    }
    if (!err.isEmpty())
    {
        KAPPS_CORE_WARNING_CATEGORY(_stderrCategory) << err.data();
    }
    return exitCode;
}


void Executor::cmdDetached(const QString &program, const QStringList &args)
{
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.startDetached();
}

#if defined(Q_OS_UNIX)
int Executor::bash(const QString &command, bool ignoreErrors)
{
    return cmdImpl(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), command},
                   traceShellCmd, {}, nullptr, ignoreErrors);
}

void Executor::bashDetached(const QString &command)
{
    cmdDetached(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), command});
}

QString Executor::bashWithOutput(const QString &command, bool ignoreErrors)
{
    QString output;
    cmdImpl(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), command},
            traceShellCmd, {}, &output, ignoreErrors);

    return output;
}
#endif

// Execute a specific program with arguments
int Executor::cmd(const QString &program, const QStringList &args, bool ignoreErrors)
{
    return cmdWithEnv(program, args, {}, ignoreErrors);
}

// Execute a specific program with arguments and an environment
int Executor::cmdWithEnv(const QString &program, const QStringList &args, const QProcessEnvironment &env, bool ignoreErrors)
{
    return cmdImpl(program, args, traceCmd, env, nullptr, ignoreErrors);
}

#if defined(Q_OS_UNIX)
QString Executor::cmdWithOutput(const QString &program, const QStringList &args)
{
    QString output;
    // Nonzero return values are traced by cmdImpl, output is empty in that case
    cmdImpl(program, args, traceCmd, {}, &output, false);
    return output;
}

QRegularExpressionMatch Executor::cmdWithRegex(const QString &program,
                                               const QStringList &args,
                                               const QRegularExpression &regex)
{
    auto output = cmdWithOutput(program, args);
    auto match = regex.match(output);
    if(!match.hasMatch())
    {
        KAPPS_CORE_WARNING_CATEGORY(_category) << "No match in output from"
            << CmdTrace{program, args, traceCmd};
        KAPPS_CORE_WARNING_CATEGORY(_category) << qUtf8Printable(output);
    }
    return match;
}
#endif

namespace Exec
{
    Executor &defaultExecutor()
    {
        static kapps::core::LogCategory _defaultCategory{__FILE__, "process"};
        static Executor _defaultExecutor{_defaultCategory};
        return _defaultExecutor;
    }

#if defined(Q_OS_UNIX)
    int bash(const QString &command, bool ignoreErrors)
    {
        return defaultExecutor().bash(command, ignoreErrors);
    }

    void bashDetached(const QString &command) {
        defaultExecutor().bashDetached(command);
    }

    QString bashWithOutput(const QString &command, bool ignoreErrors)
    {
        return defaultExecutor().bashWithOutput(command, ignoreErrors);
    }

    QString cmdWithOutput(const QString &program, const QStringList &args)
    {
        return defaultExecutor().cmdWithOutput(program, args);
    }

    QRegularExpressionMatch cmdWithRegex(const QString &program,
                                         const QStringList &args,
                                         const QRegularExpression &regex)
    {
        return defaultExecutor().cmdWithRegex(program, args, regex);
    }
#endif

    int cmd(const QString &program, const QStringList &args, bool ignoreErrors)
    {
        return defaultExecutor().cmd(program, args, ignoreErrors);
    }

    int cmdWithEnv(const QString &program, const QStringList &args, const QProcessEnvironment &env, bool ignoreErrors)
    {
        return defaultExecutor().cmdWithEnv(program, args, env, ignoreErrors);
    }
}
