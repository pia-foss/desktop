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
#ifndef KAPPS_CORE_OS_WINDOWS

#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <functional>
#include <algorithm>
#include <string>
#include <vector>
#include <atomic>
#include <array>
#include <exception>
#include <poll.h>
#include <thread>
#include "posix/posix_objects.h"
#include "posix/posixfdnotifier.h"
#include "util.h"
#include "stringslice.h"
#include "signal.h"

// NOTE: Due to MSVC relying on an internal header called process.h, this file
// can't be called process.h.  (Or we need to stop adding src/ directories to
// include paths, and instead include something like
// "kapps-core/src/coreprocess.h", which is probably a good idea as our number of
// modules grows.)

namespace kapps { namespace core {

// Start a child process, with:
// - optional arguments
// - optional environment
// - output from stdout/stdin
// - exit code
//
// Process can't currently be detached from the actual child process - it must
// always wait for the child process to exit before Process is destroyed.
//
// *************************
// * Synchronous execution *
// *************************
//
// Use run() to run and wait for the child process to exit:
//
// ```
// // If you need the exit code, etc.:
// Process deleteFile{"rm", {"/tmp/file.tmp"}};
// deleteFile.run(discardSinkFunc(), discardSinkFunc());
// // Observe exitCode(), etc.
// ```
//
// The above method discards stderr/stdout data.  It's still important to
// provide a sink functor if the process may write to those streams.
//
// **************************
// * Asynchronous execution *
// **************************
//
// Use start() to start the process asynchronously.  The stdout and stderr
// streams are monitored with PosixFdNotifier, so this requires an event loop
// integration (see kapps::core::EventLoop - this is provided by PollThread and
// by the product on the main thread).
//
// ```
// Process deleteDir{"rm", {"-rf", "/tmp/my_product_tmp"}};
// deleteDir.start(discardSinkFunc(), discardSinkFunc(), [this]{deleteDone();});
// // Later, in deleteDone, observe the exit code with exitCode(), etc.
// ```
//
// Like with run(), it's important to provide sink functors if the process may
// write to those streams, so Process can open a pipe for them.
//
// NOTE: Asynchronous execution relies on detecting a hangup on stderr/stdout
// to detect when the process is exiting - it will then call waitpid() to reap
// the exit code.  At least one stream MUST be opened to actually run
// asyncrhonously.  (Otherwise there is no hangup to detect, so Process calls
// waitpid() immediately.)
//
// **********
// * Output *
// **********
//
// To capture output, provide a ready functor for start()/run() to invoke when
// stdout or stderr has data to read, and read the data there.  Several standard
// sinks are provided, or you can create a custom one.
//
// To capture all output into a std::string, use a StringSink:
//
// ```
// Process hugePs{"ps", {"-ALf"}};
// StringSink out, err;
// hugePs.start();
// hugePs.wait(out.readyFunc(), err.readyFunc());
// // Then observe out.data() and err.data()
// ```
//
// If you don't care about output, use discardSinkFunc():
//
// ```
// Process noisyDaemon{"my-noisyd", {"--foreground"}};
// noisyDaemon.start();
// noisyDaemon.wait(discardSinkFunc(), discardSinkFunc());
// ```
//
// ***************
// * Termination *
// ***************
//
// Process will always wait in some form for the child process to exit.  If
// Process is being destroyed while running asynchronously, its destructor sends
// SIGTERM and then enters a synchronous wait for the child to exit.
//
// There is intentionally no way to abandon a child process - on UNIX, the
// parent must _always_ reap the exit code, otherwise a zombie is left until the
// parent exits.
//
// If a caller needs to be able to "abandon" a process that was started
// asynchronously, you must move the Process somewhere that it can continue to
// wait for the child to exit (say, a queue of old Process objects, etc.).
//
// (This is different from starting detached in the first place, which is
// possible without having to keep a Process object.  This is not currently
// provided, but could be implemented with a double-fork: fork once, then do the
// rest of startChild() from the first fork (which forks again), then
// immediately exit the first fork without waiting.)
//
class KAPPS_CORE_EXPORT Process
{
private:
    enum : int
    {
        // Used as _exit(status) value in child process
        // when the exec() failed
        ExecFailed = 204,
        // Used as _exitCode if the child terminated due to a signal
        ExecSignalTerminated = 205,
        // Used as _exitCode if any other error occurs, such as a failure to
        // waitpid() or some unexpected process status
        ExecOtherError = 206
    };

public:
    // A ReadyReadFunc is invoked when waiting for the process to exit and
    // either stdout/stderr has data available.  The functor must read from the
    // file descriptor and return true/false indicating whether the file
    // descriptor is still connected (true) or the remote side was hung up
    // (false).  (Note that PosixFd::appendAll() and PosixFd::discardAll()
    // return this.)
    using ReadyReadFunc = std::function<bool(PosixFd&)>;

public:
    // Create a process with the executable, arguments, and environment.
    // The process must be a path (absolute or relative), this does not search
    // PATH (this uses execve()).
    Process(std::string pathName, std::vector<std::string> args,
            std::vector<std::string> env);
    // Create a process with executable and arguments.  The current environment
    // is used.
    // The process can be a path (absolute or relative), or an executable name
    // found in PATH (this uses execvp()).
    Process(std::string pathName, std::vector<std::string> args);
    // If a child process still exists, Process's destructor sends SIGTERM and
    // then waits for it to exit.  Any remaining (or new) stdout or stderr data
    // are discarded.
    //
    // ~Process() logs a warning in this case, as this is usually not intended;
    // the caller should usually wait to observe the result.
    ~Process();

private:
    // Wait for the process to exit.  While waiting, if stdout or stderr data
    // becomes available, stdoutReadyRead or stderrReadyRead are invoked (on
    // this thread).
    //
    // If you don't read all available data during std{out,err}ReadyRead,
    // they'll be called again (poll() is level-triggered.)
    //
    // NOTE: If you don't provide stdoutReadyRead or stderrReadyRead, then that
    // stream is not polled:
    // - If you know the output will fit in the OS pipe buffer, you can do this
    //   to wait for exit and then use readAll*() to read the buffer.
    // - If the output *does* exceed the buffer, the child process may block
    //   attempting to write more output, which means it will never exit.
    // - If neither functor is provided, this essentially turns into waitpid().
    //
    // If stdout or stderr could fill the pipe buffer, consider using a
    // StringSink to capture the output to a string, or discardSinkFunc() to
    // ignore it.
    void wait(ReadyReadFunc stdoutReadyRead, ReadyReadFunc stderrReadyRead);

    void execChild(PosixFd stdoutWriteEnd, PosixFd stderrWriteEnd);

public:
    int exitCode() const;

private:
    // Wait for the child process to exit with waitpid() after observing hangups
    // on both stdout/stderr
    void waitPidExit();
    bool receive(ReadyReadFunc &stdoutReadyRead, ReadyReadFunc &stderrReadyRead,
                 std::chrono::milliseconds timeout);
    void throwIfRunning();
    // Start the child process.  If a child process is already running, this
    // throws.
    // Process can be started again once the child terminates; exitCode()
    // continues to return the old exit code until the new child terminates.
    void startChild();
    // When running asynchronously, if both stdout/stderr have been hung up,
    // wait for exit and then invoke _exitFunc.
    void waitIfAsyncHangup();

public:
    const std::string pathName() const {return _pathName;}
    const std::vector<std::string> arguments() const {return _args;}
    bool running() const {return _childPid;}
    bool runningAsync() const;
    int pid() const {return _childPid;}

    // Start the process and run synchronously.
    // - If a child process is already running, this throws.
    // - The Process can be started again once the process terminates.
    // - run() waits for the process to exit.  While waiting, if stdout or
    //   stderr data become available (or the stream is hung up), the ready-read
    //   functor is invoked.  If you do not read all available data, they will
    //   be invoked again.
    // - If either ready-read functor is not provided, that stream is NOT opened
    //   in the client process.  (Process doesn't provide a default stream
    //   and we don't want to risk the child process deadlocking due to filling
    //   up the OS pipe buffer.)
    // - Once both streams are hung up, run() reaps the process exit code with
    //   waitpid().  If neither ready-read functor was given, this essentially
    //   invokes waitpid() immediately.
    //
    // Because this is synchronous, the ready-read functors can capture refs
    // to the calling environment.
    void run(ReadyReadFunc stdoutReadyRead, ReadyReadFunc stderrReadyRead);

    // Start and run the process asynchronously.
    // - If a child process is already running, this throws.
    // - The Process can be started again once the process terminates.
    // - When the process exits, exitFunc() will be invoked.  Callers should
    //   virtually always provide an exit functor to be notified of exit.
    // - Until the process exits, if stdout or stderr data become available (or
    //   the stream is hung up), the ready-read functor is invoked.  If you do
    //   not read all available data, they will be invoked again.
    // - If either ready-read functor is not provided, that stream is NOT opened
    //   in the client process, due to the same deadlock risk as in run().  If
    //   _neither_ functor is provided, this becomes synchronous, as there is no
    //   stream hangup event to detect.
    // - Once both streams are hung up, start() reaps the process exit code with
    //   waitpid().  For most processes, this is fine as stream hangup indicates
    //   that the process is exiting.  Some unusual processes might hangup or
    //   reopen their own stdout/stderr early - if this is the case (for both
    //   streams), start() will block between the last hangup and the actual
    //   process exit.
    //
    // If start() completes, exitFunc _will_ eventually be invoked (even if the
    // process couldn't be executed due to a failed exec(), etc.).  If we are
    // unable to create the child process at all (fork() fails), then this
    // throws, and exitFunc will _not_ be invoked.
    //
    // (Note that this differs from QProcess but makes it clearer whether the
    // caller can expect exitFunc to be invoked or not.  With QProcess, some
    // failure modes return successfully, emit an error signal, and do not emit
    // an exit signal.  Other failure modes return successfully, emit an error,
    // but then still emit an exit signal too.
    //
    // Because this is asynchronous, the ready-read functors cannot generally
    // capture refs to the calling environment.  Generally, the Process should
    // be owned by an object, which allows you to safely cature this in the
    // functors.
    void start(std::function<void()> exitFunc, ReadyReadFunc stdoutReadyRead,
               ReadyReadFunc stderrReadyRead);

    // A process started asynchronously with start() can be convered to a
    // synchronous wait.  The exitFunc and ready-read functors passed to start()
    // are discarded, and the new ready-read functors are used instead.
    //
    // If the Process is not currently running asynchronously, this throws (a
    // synchronous wait cannot be started during a callback from a synchronous
    // Process).
    //
    // If a particular ready-read functor is not provided, Process immediately
    // hangs up that stream, since it is no longer being monitored. (Otherwise,
    // the child could deadlock if it fills the pipe buffer.)  Use
    // discardSinkFunc() if you want to ignore incoming data without hanging up
    // the pipe.
    //
    // ~Process() does this but must discard any stdout/stderr data that are
    // received.  A higher-level abstraction built on Process may wish to do
    // this in its own destructor so it can capture any remaining output from
    // the process.
    void waitForExit(ReadyReadFunc stdoutReadyRead,
                     ReadyReadFunc stderrReadyRead);

    // Send a signal to the process.  Throws if not running.
    // This can be used while running synchronously or asynchronously, including
    // from a ready-read functor.  If the process is not running, this throws.
    void signal(int signal);

    // Send a SIGTERM signal to the process (does not wait for it to exit).
    void terminate() {signal(SIGTERM);}

    // Send a SIGKILL signal to the process (does not wait for it to exit).
    // Like terminate(), this can be used while running synchronously or
    // asynchronously, including from ready-read functors.  Throws if not
    // running.
    void kill() {signal(SIGKILL);}

public:
    // Signal emitted to set up the child process.  This is emitted between
    // fork() and exec() - i.e. in the child process.  start() and run() never
    // return in the child process (they either exec() or exit()), so if the
    // caller hooks this up just before start()/run(), it can capture references
    // to the environment.  (In that case, clear it out again after
    // start()/run() returns in the parent process.)
    Signal<> prepareChildProcess;

private:
    // *********
    // * Setup * - Configures the process to be executed (from ctor)
    // *********

    // Executable path/name.  Currently, can be a name (searched in PATH) only
    // when no environment is specified - otherwise must be a path (absolute or
    // relative).
    std::string _pathName;
    // Process arguments, does not include argv[0] (can be empty)
    std::vector<std::string> _args;
    // Process environment, only meaningful when Process was constructed with an
    // environment (indicated by _withEnv)
    std::vector<std::string> _env;
    bool _withEnv;

    // ***********
    // * Running * - These indicate that we own resources associated with a
    // ***********   running process.  Once all of these are cleared, the
    //               process has exited.  Note that the pipes can be cleared
    //               before _childPid is cleared.

    // stdout and stderr pipe read ends - used to receive data from the new
    // process.  The write ends are held only by the child process.  Once the
    // child hangs up either of these pipes, the PosixFd is cleared.
    PosixFd _stdoutReadEnd;
    PosixFd _stderrReadEnd;
    // The current child PID.  This is set while a child process exists (it's
    // cleared once we reap the child with waitpid().)
    //
    // Note that because we have to reap the exit code to allow the kernel to
    // destroy a zombie process, this indicates "ownership" of the resource
    // underlying the process - we have to clean it up with waitpid().
    int _childPid;
    // * Async only * - the PosixFdNotifiers used for stderr/stdout.  These are
    // only constructed when calling start() so that run() can be used without
    // requiring an event loop integration.  Synchronous execution uses a
    // special-purpose poll(2) loop instead.
    nullable_t<PosixFdNotifier> _pStdoutNotifier, _pStderrNotifier;
    // * Async only * - the exit functor to invoke.  There's no need for an
    // exit functor when executing synchronously.
    std::function<void()> _exitFunc;

    // **********
    // * Exited * - The result of an exited process.
    // **********
    int _exitStatus;
};

// String sink - sinks all available data from a process's stdout or stderr to
// a single string.
class KAPPS_CORE_EXPORT StringSink
{
public:
    // Get the ready-read functor for this StringSink
    std::function<bool(PosixFd&)> readyFunc();

    // Get or take the contained data
    const std::string &data() const & {return _data;}
    std::string data() && {return std::move(_data);}

private:
    std::string _data;
};

// Create a discard sink for a process's stdout or stderr; this just discards
// the data so there is no associated state.
std::function<bool(PosixFd&)> discardSinkFunc();

// Line sink - sinks all available data from a process's stdout or stderr, then
// emits each line as it is completed.
//
// Either LF or CRLF line endings are accepted at the end of each line.
class KAPPS_CORE_EXPORT LineSink
{
public:
    // Get the ready-read functor for this LineSink
    std::function<bool(PosixFd&)> readyFunc();

    // Reset the LineSink, such as to reuse it with another process invocation.
    // If any partial line was left in the buffer, it is returned.
    std::string reset();

public:
    // Emitted whenever a complete line is received.  The line endings are not
    // included.
    Signal<StringSlice> lineComplete;

private:
    std::string _buffer;
};

}}

#endif
