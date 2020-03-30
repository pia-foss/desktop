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
#line SOURCE_FILE("exec.cpp")

#include "exec.h"
#include <QProcess>

namespace
{
    // Tracer for cmd() and friends
    const auto traceCmd = [](QDebug &dbg, const QString &program, const QStringList &args)
    {
        dbg << program;
        for(const auto &arg : args)
            dbg << ' ' << arg;
    };
}

Executor::Executor(const QLoggingCategory &category)
    : _category{category},
      _stdoutCatName{buildSubcategoryName(category, QLatin1String{".stdout"})},
      _stderrCatName{buildSubcategoryName(category, QLatin1String{".stderr"})},
      _stdoutCategory{_stdoutCatName.data()},
      _stderrCategory{_stderrCatName.data()}
{
}

QByteArray Executor::buildSubcategoryName(const QLoggingCategory &category,
                                          const QLatin1String &suffix)
{
    QByteArray subcatName;
    QLatin1String catName{category.categoryName()};
    subcatName.reserve(catName.size() + suffix.size());
    subcatName.append(catName.data(), catName.size());
    subcatName.append(suffix.data(), suffix.size());
    return subcatName;
}

// bash - $ command
// cmd - program args

// This seems a bit excessive, but the only way to write into a QDebug from
// qCWarning() and friends is with some sort of postfix expression, due to the
// way the macros work.  This is an object we can insert that just calls the
// functor with appropriate arguments when traced.
struct CmdTrace
{
    using FuncT = void(*)(QDebug&, const QString&, const QStringList&);
    const QString &program;
    const QStringList &args;
    FuncT pFunc;
};

QDebug &operator<<(QDebug &dbg, const CmdTrace &trace)
{
    Q_ASSERT(trace.pFunc);    // Ensured by caller
    trace.pFunc(dbg, trace.program, trace.args);
    return dbg;
}

int Executor::cmdImpl(const QString &program, const QStringList &args,
                      void(*traceFunc)(QDebug &, const QString&, const QStringList&),
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
        qCWarning(_category).nospace() << "(" << exitCode << ") "
            << CmdTrace{program, args, traceFunc};
    }
    if (!out.isEmpty())
    {
        qCInfo(_stdoutCategory).noquote() << out;
    }
    if (!err.isEmpty())
    {
        qCWarning(_stderrCategory).noquote() << err;
    }
    return exitCode;
}

#if defined(Q_OS_UNIX)
int Executor::bash(const QString &command, bool ignoreErrors)
{
    auto traceShellCmd = [](QDebug &dbg, const QString&, const QStringList &args)
    {
        // Just trace "$ <command>".  This is
        // Turn off quoting, don't need to bother saving state (this is tightly
        // coupled to cmdImpl)
        dbg.noquote();
        dbg << "$ " << args[1];
    };
    return cmdImpl(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), command},
                   traceShellCmd, {}, nullptr, ignoreErrors);
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
        qCWarning(_category).noquote() << "No match in output from"
            << CmdTrace{program, args, traceCmd};
        qCWarning(_category).noquote() << output;
    }
    return match;
}
#endif

namespace Exec
{
    Executor &defaultExecutor()
    {
        static QLoggingCategory _defaultCategory{"process"};
        static Executor _defaultExecutor{_defaultCategory};
        return _defaultExecutor;
    }

#if defined(Q_OS_UNIX)
    int bash(const QString &command, bool ignoreErrors)
    {
        return defaultExecutor().bash(command, ignoreErrors);
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
