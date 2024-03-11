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

#include <kapps_core/core.h>
#ifndef KAPPS_CORE_OS_WINDOWS

#include "coreprocess.h"
#include "logger.h"
#include <signal.h>
#include <sys/wait.h>

namespace kapps { namespace core {

Process::Process(std::string pathName, std::vector<std::string> args,
                 std::vector<std::string> env)
    : _pathName{std::move(pathName)},
      _args{std::move(args)},
      _env{std::move(env)},
      _withEnv{true},
      _childPid{0},
      _exitStatus{-1}
{
}

Process::Process(std::string pathName, std::vector<std::string> args)
    : Process(std::move(pathName), std::move(args), {})
{
    _withEnv = false;
}

Process::~Process()
{
    if(_childPid)
    {
        KAPPS_CORE_WARNING() << "Process being destroyed while still running in PID"
            << _childPid << "-" << _pathName << "-" << _args;
        // Tell the process that it should really shut down now - we don't kill
        // it though (risky without knowing exactly what the process is), if it
        // does not respond to SIGTERM the parent will hang.
        terminate();
        // Convert the async process to a synchronous wait here in the
        // destructor.  Discard any remaining data that comes in.  This is the
        // most friendly in general - we don't hang up the stdout/stderr pipes
        // which many processes probably are not prepared for.  We don't reuse
        // the existing ready-read functors as they are probably not valid once
        // Process is being destroyed.
        //
        // This also reaps the exit code, which avoids zombies.
        waitForExit(discardSinkFunc(), discardSinkFunc());
    }
}

void Process::wait(ReadyReadFunc stdoutReadyRead, ReadyReadFunc stderrReadyRead)
{
    // If wait() is called without actually starting a process, this results
    // in an error exit code being set (as intended):
    // - receive() returns false immediately due to no open pipes
    // - waitPidExit() tries to wait on PID 0 and receives an error

    // Just receive all data until both streams are hung up, then wait for the
    // process to exit.
    while(receive(stdoutReadyRead, stderrReadyRead, std::chrono::milliseconds{-1}));
    waitPidExit();
}

void Process::execChild(PosixFd stdoutWriteEnd, PosixFd stderrWriteEnd)
{
    // Set stdout/stderr to the write end of the pipes
    NO_EINTR(dup2(stdoutWriteEnd.get(), STDOUT_FILENO));
    NO_EINTR(dup2(stderrWriteEnd.get(), STDERR_FILENO));

    // The +2 reserves space for
    // (1) the first argument (name of program)
    // (2) the nullptr to indicate end of arg array
    std::vector<char*> cArgs{_args.size() + 2};
    // +1 for nullptr indicating end of env array
    std::vector<char*> cEnv{_env.size() + 1};

    // First element in args for exec is the name of the program
    // NOT the first argument
    cArgs[0] = &_pathName[0];

    std::transform(_args.begin(), _args.end(), cArgs.begin() + 1, [](const std::string& str){return const_cast<char*>(str.c_str());});
    std::transform(_env.begin(), _env.end(), cEnv.begin(), [](const std::string& str){return const_cast<char*>(str.c_str());});

    int execReturn{-1};
    if(_withEnv)
    {
        // TODO - We should probably search PATH here too, but macOS doesn't
        // support execvpe()
        execReturn = ::execve(_pathName.c_str(), cArgs.data(), cEnv.data());
    }
    else
    {
        // use execvp() rather than execv() so PATH is used
        // to find executable file names
        execReturn = ::execvp(_pathName.c_str(), cArgs.data());
    }

    // Note that this goes to the child process's stdout, which the parent
    // should capture and trace.  It's generally not a good idea to use
    // KAPPS_CORE_WARNING(), etc. from the child process, as in some cases both
    // processes might try to rotate logs at the same time, etc.
    std::cerr << "Error running process (" << execReturn << ")" << cArgs[0]
        << ErrnoTracer{};
    // We have to exit; execChild() cannot return in the child process.  This
    // result is picked up by the parent process.
    //
    // NOTE: If the intended child process also uses this exit code, we can't
    // necessarily differentiate a failure to exec.  We might be able to use an
    // additional pipe to specifically indicate the exec result.
    _exit(ExecFailed);
}

int Process::exitCode() const
{
    // Normal exit
    if(WIFEXITED(_exitStatus))
    {
        // Extract out the exit status for a normal exit
        if(WEXITSTATUS(_exitStatus) == ExecFailed)
            KAPPS_CORE_WARNING() << "Child process failed: ExecFailed";

        return WEXITSTATUS(_exitStatus);
    }
    // Abnormal exit due to signal
    else if(WIFSIGNALED(_exitStatus))
    {
        // Extract out the signal code that caused an abnormal exit.
        // This also includes exits due to core dumps (WCOREDUMP)
        // TODO: Indicate that an abnormal exit (via signal) happened
        // Don't confuse signal codes with possibly equal results from the
        // process, use a specific code.
        return ExecSignalTerminated;
    }
    // Should never reach here since there are only two kinds of exits (above)
    else
    {
        // This status is unexpected, it could be something like WIFSTOPPED
        // that we're not handling
        KAPPS_CORE_WARNING() << "Unexpected status from child process:"
            << _exitStatus;
        return ExecOtherError;
    }
}

void Process::waitPidExit()
{
    int ret{-1};
    NO_EINTR(ret = ::waitpid(_childPid, &_exitStatus, 0));

    // We've now reaped _childPid, so clear it - the PID could be reused at this
    // point
    _childPid = 0;

    if(ret == -1)
    {
        KAPPS_CORE_WARNING() << "::waitpid failed with" << ErrnoTracer{};

        // Something very odd has happened, could be the status has already been reaped
        // by another part of the daemon calling and indiscriminate wait()
        // in any case this is very unusual so let's set the _exitStatus to indicate an error
        // TODO - Use something different here, we don't know for sure how
        // WIFEXITED(1), etc. will behave.
        _exitStatus = 1;
    }
    // Process died due to signal, trace it
    else if(WIFSIGNALED(_exitStatus))
    {
        // Extract out the signal code that caused an abnormal exit.
        // This also includes exits due to core dumps (WCOREDUMP)
        // TODO: Indicate that an abnormal exit (via signal) happened
        KAPPS_CORE_WARNING() << "Child process exited due to signal:" << WTERMSIG(_exitStatus) << (WCOREDUMP(_exitStatus) ? "core dumped" : "");
    }
}

bool Process::receive(ReadyReadFunc &stdoutReadyRead, ReadyReadFunc &stderrReadyRead,
                      std::chrono::milliseconds timeout)
{
    enum PollIdx : std::size_t
    {
        StdoutPipe,
        StderrPipe,
        Count
    };

    // If stdout or stderr is not being monitored, hang up the pipe.  Since we
    // can't do anything with data on the pipe, the child process could
    // otherwise deadlock if it filled the pipe buffer.
    if(!stdoutReadyRead)
        _stdoutReadEnd.close();
    if(!stderrReadyRead)
        _stderrReadEnd.close();

    // Poll stdout and stderr - whichever are still open
    std::array<pollfd, PollIdx::Count> fds{};
    fds[PollIdx::StdoutPipe] = {_stdoutReadEnd.get(), POLLIN, 0};
    fds[PollIdx::StderrPipe] = {_stderrReadEnd.get(), POLLIN, 0};

    // If there is nothing to poll, we're done - the caller should not call
    // receive() again, move on to waitPidExit().
    // This covers any combination of:
    // - stdout/stderr having been hung up by the child process (we hung them
    //   up earlier when observing this condition)
    // - stdout/stderr hung up on our side due to lack of a callback
    if(fds[PollIdx::StdoutPipe].fd < 0 && fds[PollIdx::StderrPipe].fd < 0)
        return false;

    int pollResult{-1};
    NO_EINTR(pollResult = ::poll(fds.data(), fds.size(), timeout.count()));

    if(pollResult > 0)
    {
        // Handle signaled pipes.
        // If either pipe has been hung up, close its file descriptor to
        // indicate that we've handled the hangup.  If both are now closed, the
        // next receive() will return false, and then we'll wait for the
        // process to exit.
        if(fds[PollIdx::StdoutPipe].revents)
        {
            assert(stdoutReadyRead);    // Consequence of above
            if(!stdoutReadyRead(_stdoutReadEnd))
                _stdoutReadEnd.close(); // Remote side hung up
        }
        if(fds[PollIdx::StderrPipe].revents)
        {
            assert(stderrReadyRead);    // Consequence of above
            if(!stderrReadyRead(_stderrReadEnd))
                _stderrReadEnd.close(); // Remote side hung up
        }
     }
     else if(pollResult == -1)
         KAPPS_CORE_WARNING() << "Poll failed: " << ErrnoTracer{};

    // One or more of the following happened:
    // - stdin/stderr had data to read
    // - stdin/stderr hung up
    // - timeout elapsed
    //
    // In any event, the caller can call receive() again.  If both pipes have
    // been hung up (or are being ignored, or any combination), then receive()
    // will return false next time.
    return true;
}

void Process::throwIfRunning()
{
    if(_childPid)
        throw std::runtime_error("Cannot call Process::start() while already running");
}

void Process::startChild()
{
    // Don't touch any members until we know fork() succeeded.  If it fails, or
    // anything before that fails (like createPipe()), we throw and remain in
    // the NotStarted state.

    // We need to inherit the write ends across exec() so suppress FD_CLOEXEC
    // initially and only apply it to the read ends.
    PosixPipe stdoutPipe = createPipe(false);
    PosixPipe stderrPipe = createPipe(false);

    // Non-blocking reads from pipe, also close read ends in child process
    stdoutPipe.readEnd.applyNonblock().applyClOExec();
    stderrPipe.readEnd.applyNonblock().applyClOExec();
    // Write ends also close on exec, because we duplicate those over to the
    // stdout/stderr file descriptors before executing the child
    stdoutPipe.writeEnd.applyClOExec();
    stderrPipe.writeEnd.applyClOExec();

    int childPid = ::fork();

    if(childPid < 0)
    {
        KAPPS_CORE_WARNING() << "Unable to start" << _pathName
            << "- fork failed with result" << childPid << "-" << ErrnoTracer{};
        throw std::runtime_error("Could not fork");
    }
    else if(childPid == 0)
    {
        // Give the caller a chance to set up any specific requirements in the
        // child process, such as changing UID/GID, etc.
        prepareChildProcess();
        execChild(std::move(stdoutPipe.writeEnd), std::move(stderrPipe.writeEnd));
    }
    else
    {
        _childPid = childPid;
        // Hang on to the read ends of the pipes.  The write ends will be closed
        // by the PosixFds in the Pipe objects.
        std::swap(_stdoutReadEnd, stdoutPipe.readEnd);
        std::swap(_stderrReadEnd, stderrPipe.readEnd);
    }
}

void Process::waitIfAsyncHangup()
{
    if(!_stdoutReadEnd && !_stderrReadEnd)
    {
        waitPidExit();
        // Invoking _exitFunc might destroy this.  If it doesn't, we still want
        // to be sure _exitFunc is destroyed right after.  Pull it out and then
        // invoke the local copy.
        std::function<void()> exitFunc;
        std::swap(exitFunc, _exitFunc);
        if(exitFunc)
            exitFunc();
    }
}

bool Process::runningAsync() const
{
    // When running asynchronously, at least one of _pStdoutNotifier or
    // _pStderrNotifier is set.
    //
    // The exception is in the "endgame" state where we call waitpid()
    // synchronously after observing hangups on both stdout/stderr.  However,
    // this state isn't observable - waitpid() occurs immediately after hanging
    // up our last pipe, and no callbacks can occur from that point until the
    // process exits.
    return _pStdoutNotifier || _pStderrNotifier;
}

void Process::run(ReadyReadFunc stdoutReadyRead, ReadyReadFunc stderrReadyRead)
{
    throwIfRunning();
    startChild();
    wait(std::move(stdoutReadyRead), std::move(stderrReadyRead));
}

void Process::start(std::function<void()> exitFunc, ReadyReadFunc stdoutReadyRead,
                    ReadyReadFunc stderrReadyRead)
{
    throwIfRunning();

    assert(!_pStdoutNotifier);  // Class invariant, clear when not running
    assert(!_pStderrNotifier);  // Class invariant, clear when not running

    // If the caller intends to monitor stdout/stderr, we need an event loop
    // integration.  If that isn't present, make sure PosixFdNotifier throws
    // here before we start the process.
    if(stdoutReadyRead)
        _pStdoutNotifier.emplace();
    if(stderrReadyRead)
        _pStderrNotifier.emplace();

    try
    {
        startChild();
    }
    catch(...)
    {
        // Since we were unable to start, clear the fd notifiers
        _pStdoutNotifier.clear();
        _pStderrNotifier.clear();
    }

    // The process was started, store the exitFunc and hook up the notifiers
    _exitFunc = std::move(exitFunc);
    if(_pStdoutNotifier)
    {
        _pStdoutNotifier->activated = [this, rrFunc=std::move(stdoutReadyRead)]
        {
            if(!rrFunc(_stdoutReadEnd))
            {
                _pStdoutNotifier.clear();
                _stdoutReadEnd.close(); // Remote hung up
            }
            // Once both are hung up, wait for exit
            waitIfAsyncHangup();
        };
        _pStdoutNotifier->set(_stdoutReadEnd.get(), PosixFdNotifier::WatchType::Read);
    }
    else
    {
        // Not monitoring stdout - hang up our pipe
        _stdoutReadEnd.close();
    }
    if(_pStderrNotifier)
    {
        _pStderrNotifier->activated = [this, rrFunc=std::move(stderrReadyRead)]
        {
            if(!rrFunc(_stderrReadEnd))
            {
                _pStderrNotifier.clear();
                _stderrReadEnd.close(); // Remote hung up
            }
            // Once both are hung up, wait for exit
            waitIfAsyncHangup();
        };
        _pStderrNotifier->set(_stderrReadEnd.get(), PosixFdNotifier::WatchType::Read);
    }
    else
    {
        // Not monitoring stderr - hang up our pipe
        _stderrReadEnd.close();
    }
}

void Process::waitForExit(ReadyReadFunc stdoutReadyRead, ReadyReadFunc stderrReadyRead)
{
    // Check running() and runningAsync() separately to improve tracing
    if(!running())
    {
        KAPPS_CORE_WARNING() << "Can't start synchronous wait on Process that is not running";
        throw std::runtime_error{"Process is not running"};
    }
    if(!runningAsync())
    {
        KAPPS_CORE_WARNING() << "Can't start synchronous wait on Process that is running synchronously";
        throw std::runtime_error{"Process is already running synchronously"};
    }

    // Convert the async process to synchronous - discard the caller's
    // ready-read and exit functors, and wait here for exit.
    _pStdoutNotifier.clear();
    _pStderrNotifier.clear();
    _exitFunc = {};
    // wait() will hang up stdout and/or stderr if no new ready-read func was
    // provided (in receive()).
    wait(std::move(stdoutReadyRead), std::move(stderrReadyRead));
}

void Process::signal(int signal)
{
    if(!running())
    {
        KAPPS_CORE_WARNING() << "Tried to send signal" << signal
            << "to a process that's not running";
        throw std::runtime_error{"Process is not running"};
    }

    ::kill(_childPid, signal);
}

std::function<bool(PosixFd&)> StringSink::readyFunc()
{
    return [this](PosixFd &fd){return fd.appendAll(_data);};
}

std::function<bool(PosixFd&)> discardSinkFunc()
{
    return [](PosixFd &fd){return fd.discardAll();};
}

std::function<bool(PosixFd&)> LineSink::readyFunc()
{
    return [this](PosixFd &fd) -> bool
    {
        auto atEnd = fd.appendAll(_buffer);

        // Emit all complete lines in _buffer
        std::size_t nextLineStart = 0;
        std::size_t nextLf = _buffer.find('\n');
        while(nextLf != StringSlice::npos)
        {
            std::size_t lineEnd = nextLf;
            // Trim CR too if the line ended with CRLF
            if(lineEnd > nextLineStart && _buffer[lineEnd-1] == '\r')
                --lineEnd;
            auto line = _buffer.substr(nextLineStart, lineEnd - nextLineStart);
            lineComplete(line);

            // Advance past the LF, if this is the end of the buffer,
            // nextLineStart now equals _buffer.size()
            nextLineStart = nextLf+1;
            // Find the next LF, if there is one
            nextLf = _buffer.find('\n', nextLineStart);
        }

        // If lines were emitted, delete all the content that was emitted.
        // This requires recopying any remaining content in the buffer, but
        // generally lines aren't long so this is fine.  Using a deque/ring
        // buffer/etc. would require other copies to emit each line as
        // contiguous data.
        if(nextLineStart > 0)
            _buffer.erase(0, nextLineStart);

        return atEnd;
    };
}

std::string LineSink::reset()
{
    std::string partialLine;
    std::swap(_buffer, partialLine);    // Empties buffer
    return partialLine;
}

}}

#endif
