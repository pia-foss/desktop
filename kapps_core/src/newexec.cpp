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

#include "newexec.h"
#include "coreprocess.h"
#include <kapps_core/core.h>
#include "util.h"

#ifndef KAPPS_CORE_OS_WINDOWS    // TODO - This should work on Windows once Process is implemented on Windows

namespace kapps { namespace core {

namespace
{
    using StringVector = Executor::StringVector;
    // Tracer for cmd() and friends
    const auto traceCmd = [](std::ostream &os, const std::string &program, const StringVector &args)
    {
        os << program;
        for(const auto &arg : args)
            os << ' ' << arg;
    };

#ifdef KAPPS_CORE_OS_POSIX
    // Tracer for bash() and friends
    const auto traceShellCmd = [](std::ostream &os, const std::string &, const StringVector &args) {
        // Just trace "$ <command>".  Don't quote the command again.
        // (program is "bash" and args[0] is "-c", don't need to see those)
        os << "$ " << args[1];
    };
#endif

    // Delete all occurrences of a value from a string (in place)
    void remove(std::string &s, StringSlice value)
    {
        // If value is empty, do nothing.  An empty string can match anywhere,
        // which would result in an infinite loop due to deleting the empty
        // string having no effect.
        if(!value)
            return;

        std::string::size_type i{0};

        while(true)
        {
            // This looks weird, but it is the correct order to find range
            // [value.data(), value.data()+value.size()) starting at offset i
            i = s.find(value.data(), i, value.size());
            if(i >= s.size())
                break;
            s.erase(i, value.size());
        }
    }

    // trim from start (in place)
    void ltrim(std::string &s)
    {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));
    }

    // trim from end (in place)
    void rtrim(std::string &s)
    {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }).base(),
                s.end());
    }

    // trim from both ends (in place)
    void trim(std::string &s)
    {
        ltrim(s);
        rtrim(s);
    }
}

Executor::Executor(const kapps::core::LogCategory &category, std::string muteError)
    : _category{category},
      _stdoutCatName{buildSubcategoryName(category, ".stdout")},
      _stderrCatName{buildSubcategoryName(category, ".stderr")},
      _stdoutCategory{*category.module(), _stdoutCatName},
      _stderrCategory{*category.module(), _stderrCatName},
      _muteError{std::move(muteError)}
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
    using FuncT = void(*)(std::ostream&, const std::string&, const StringVector&);
    const std::string &program;
    const StringVector &args;
    FuncT pFunc;
};

std::ostream &operator<<(std::ostream &os, const CmdTrace &trace)
{
    assert(trace.pFunc);    // Ensured by caller
    trace.pFunc(os, trace.program, trace.args);
    return os;
}

int Executor::cmdImpl(const std::string &program, const StringVector &args,
                      void(*traceFunc)(std::ostream &, const std::string&, const StringVector&),
                      const StringVector &env, std::string *pOut,
                      bool ignoreErrors)
{
    assert(traceFunc);    // Ensured by caller

    kapps::core::Process p{program, args};

    // Set the process environment (if provided)
    //if(!env.isEmpty()) p.setProcessEnvironment(env);

    StringSink outSink, errSink;
    p.run(outSink.readyFunc(), errSink.readyFunc());
    int exitCode = p.exitCode();
    auto out = std::move(outSink).data();
    trim(out);
    auto err = std::move(errSink).data();
    // Remove _muteError before trimming, in case there is only whitespace after
    // removing _muteError (such as between two occurrences of _muteError)
    remove(err, _muteError);
    trim(err);

    if(pOut)
    {
#if defined(Q_OS_WIN)
        // This is not likely to be reliable on Windows, output is usually in
        // the current system code page, which we have overridden with UTF-8
        // (see setUtf8LocaleCodec()).  It might work for en-US but it is likely
        // to break for other locales (in particular, interface names on Windows
        // can contain non-ASCII characters).
        assert(false);
#endif
        *pOut = out;
    }

    // Remove the _muteError (if any) before checking for non-empty error
    // output - if only the _muteError was printed, the command was successful.

    if((exitCode != 0 || !err.empty()) && !ignoreErrors)
    {
        KAPPS_CORE_WARNING_CATEGORY(_category).nospace() << '(' << exitCode
            << ')' << CmdTrace{program, args, traceFunc};
    }
    if(!out.empty())
    {
        KAPPS_CORE_INFO_CATEGORY(_stdoutCategory) << out;
    }
    if(!err.empty())
    {
        KAPPS_CORE_WARNING_CATEGORY(_stderrCategory) << err;
    }
    return exitCode;
}

#if defined(KAPPS_CORE_OS_POSIX)
int Executor::bash(const std::string &command, bool ignoreErrors)
{
    return cmdImpl("/bin/bash", {"-c", command},
                   traceShellCmd, {}, nullptr, ignoreErrors);
}

std::string Executor::bashWithOutput(const std::string &command, bool ignoreErrors)
{
    std::string output;
    cmdImpl("/bin/bash", {"-c", command},
            traceShellCmd, {}, &output, ignoreErrors);

    return output;
}
#endif

// Execute a specific program with arguments
int Executor::cmd(const std::string &program, const StringVector &args, bool ignoreErrors)
{
    return cmdWithEnv(program, args, {}, ignoreErrors);
}

// Execute a specific program with arguments and an environment
int Executor::cmdWithEnv(const std::string &program, const StringVector &args, const StringVector &env, bool ignoreErrors)
{
    return cmdImpl(program, args, traceCmd, env, nullptr, ignoreErrors);
}

#if defined(KAPPS_CORE_OS_POSIX)
std::string Executor::cmdWithOutput(const std::string &program, const StringVector &args)
{
    std::string output;
    // Nonzero return values are traced by cmdImpl, output is empty in that case
    cmdImpl(program, args, traceCmd, {}, &output, false);
    return output;
}

/* QRegularExpressionMatch Executor::cmdWithRegex(const std::string &program,
                                               const StringVector &args,
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
 */#endif

namespace Exec
{
    Executor &defaultExecutor()
    {
        static kapps::core::LogCategory _defaultCategory{__FILE__, "process"};
        static Executor _defaultExecutor{_defaultCategory};
        return _defaultExecutor;
    }

#if defined(KAPPS_CORE_OS_POSIX)
    int bash(const std::string &command, bool ignoreErrors)
    {
        return defaultExecutor().bash(command, ignoreErrors);
    }

    std::string bashWithOutput(const std::string &command, bool ignoreErrors)
    {
        return defaultExecutor().bashWithOutput(command, ignoreErrors);
    }

    std::string cmdWithOutput(const std::string &program, const StringVector &args)
    {
        return defaultExecutor().cmdWithOutput(program, args);
    }
#endif

    int cmd(const std::string &program, const StringVector &args, bool ignoreErrors)
    {
        return defaultExecutor().cmd(program, args, ignoreErrors);
    }

    int cmdWithEnv(const std::string &program, const StringVector &args, const StringVector &env, bool ignoreErrors)
    {
        return defaultExecutor().cmdWithEnv(program, args, env, ignoreErrors);
    }
}
}}

#endif
