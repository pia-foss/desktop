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
#ifndef KAPPS_CORE_OS_WINDOWS    // TODO - Should work, needs Windows backend for Process

#include "util.h"
#include "coreprocess.h"
#include "timer.h"
#include "coresignal.h"
#include "stopwatch.h"

namespace kapps { namespace core {

// RestartStrategy controls restart delays for ProcessRunner.
//
// ProcessRunner notifies RestartStrategy of start, stop, and failure events for
// the process.  RestartStrategy uses this information to determine delay times
// following a process failure and measure the failure duration.
//
// If a process runs for longer than successRunTime, it is considered
// "successful" - RestartStrategy resets its ongoing measurements and emits the
// processSucceeded() signal.
class KAPPS_CORE_EXPORT RestartStrategy
{
private:
    enum : std::chrono::milliseconds::rep
    {
        // Factor applied to delay for repeated failures
        BackoffFactor = 2
    };

public:
    struct KAPPS_CORE_EXPORT Params
    {
        // Initial and maximum delay time after the process fails.
        std::chrono::milliseconds _initialDelay, _maxDelay;
        // Amount of time the process must run to be considered "successful".  This
        // controls when the delay resets to _initialDelay and how
        // failureDuration is reported.
        std::chrono::milliseconds _successRunTime;
    };

public:
    // Initialize RestartStrategy with the timing parameters.  See the Params
    // struct for details.
    RestartStrategy(Params _params);

public:
    // Reset to the initial state; forget all failure information.
    void resetFailures();

    // The process is being started (it has been enabled, a post-failure delay
    // elapsed, etc.)
    void processStarting();

    // The process has failed.  Returns the new delay to use before restarting
    // the process.
    // Also sets failureDuration to the amount of time elapses since the
    // recent failures started occurring (how long it has been since the process
    // ran for _successRunTime continuously).
    std::chrono::milliseconds processFailed(std::chrono::milliseconds &failureDuration);

public:
    // When the process has run for longer than _successRunTime, this signal is
    // emitted.
    Signal<> processSucceeded;

private:
    // Configured timing parameters
    const Params _params;
    // Next delay to use if the process fails again before _successDeadline
    // elapses.
    std::chrono::milliseconds _nextDelay;
    // This timer is started when the process is started; if it elapses, the
    // process has succeeded.
    Timer _successTimer;
    // Time since the last successful run failed.
    Stopwatch _lastSuccessEnd;
};

// ProcessRunner manages an automatically-restarted subprocess.  This can be
// used to manage subprocesses that provide a connection-oriented service, like
// a local DNS server, etc.  Most generic ongoing processes can be managed by
// ProcessRunner.
//
// ProcessRunner stores the program path and arguments to the process.
// Additional initialization can be done by overriding setupProcess(), which is
// called between setting the program/arguments and starting the process.
//
// Initially, ProcessRunner is in the "disabled" state; the process is not
// needed.  When enabled, ProcessRunner will start the process and then restart
// it if it ever fails.
//
// If the process fails, ProcessRunner emits the failed() signal and restarts
// it.
//
// Use setName() to name the object; its name is traced with any diagnostics.
class KAPPS_CORE_EXPORT ProcessRunner
{
public:
    // There is still some baggage in ProcessRunner inherited from the
    // QProcess-based implementation:
    // - We use a 0-ms exit timer to avoid destroying the Process during its
    //   exit signal.  This could be eliminated, Process supports this.
    // - We keep our own copies of the program/arg strings and rebuild the
    //   Process each time it's started.  Process supports restarting, this
    //   isn't needed.

    enum class State
    {
        // Idle - Process is in the expected state, waiting for something to
        // happen.
        // _pProcess is set when enabled; clear when disabled.
        // (If ProcessRunner is enabled, the process is running.  If it is
        // disabled, the process is not running.  Either way, the exit signal is
        // unexpected.)
        Idle,
        // Exiting - Process is being told to exit, the exit signal is
        // expected.
        // _pProcess is always set in this state.
        // ProcessRunner could be enabled or disabled at this point (enabled
        // occurs if it is disabled, then re-enabled while the process is still
        // exiting).
        Exiting,
        // Waiting - Process has exited and we're waiting to either clean up or
        // restart it.
        // _pProcess is always valid in this state; it's the last process that
        // was started (historically, we were unable to destroy it immediately).
        // _postExitTimer is running in this state.
        Waiting,
    };

public:
    ProcessRunner(RestartStrategy::Params restartParams);
    // The destructor waits for the process to exit.  (If ProcessRunner was
    // enabled, it also signals the process to exit.)  It does not emit signals
    // from its destructor.
    ~ProcessRunner();

private:
    // Start the process.  Called in the Idle state with _enabled==true and
    // with _pProcess empty.
    void startProcess();

    // Handle process exit (from processFinished()) - historically QProcess also
    // emitted errors that might mean it won't emit a finished signal.  (Process
    // always emits an exit signal if it starts successfully.)
    void handleProcessEnded();

    // Process has finished.
    void processFinished();

    // Post-exit timer has elapsed
    void postExitTimerElapsed();

public:
    void setName(std::string name) {_name = std::move(name);}
    const std::string &name() const {return _name;}

    // Enable the process.  Specify the command and arguments - if it is already
    // running and the command/arguments change, it will restart the process.
    // (The process is killed with disable().)
    //
    // The return value indicates whether the process is being started or
    // restarted as a result of this call - false indicates that the program/
    // arguments did not change, and the process will not be restarted.
    //
    // The caller must be prepared for the process to die and restart at any
    // time, but if enable() returns true, the caller _knows_ that the process
    // is restarting and may want to wait on the next startup before doing
    // anything else.  If enable() returns false, the caller may not receive
    // started() and initial program output (but it still could if the process
    // crashes or exits).
    //
    // The first time the ProcessRunner is enabled, started() is emitted
    // synchronously during this call.  This is not necessarily the case for
    // later calls to enable() (ProcessRunner could be waiting on a prior
    // process to exit, etc.)
    bool enable(std::string program, std::vector<std::string> arguments);
    // Disable the process.  If it was running, the process is killed with
    // kill().
    void disable();

    bool isEnabled() const {return _enabled;}

    // Terminate the process if it is running - if the ProcessRunner is enabled,
    // the process will be restarted (the process exit will be unexpected).
    //
    // Returns true if the process was running and a signal was sent.  This does
    // not necessarily guarantee that the process will exit, but if it does,
    // ProcessRunner will emit failed().
    //
    // Used to allow the process to shut down cleanly; the caller can implement
    // a clean shutdown by disabling the ProcessRunner when the process stops.
    //
    // This uses Process::terminate(), which sends WM_CLOSE on Windows or
    // SIGTERM on Unix.
    bool terminate();

    // Kill the process if it is running - if ProcessRunner is enabled, the
    // process will be restarted (this is treated as an unexpected failure).
    // Usually used when an error is detected that may not cause the process to
    // exit.
    //
    // This uses Process::kill(), which uses TerminateProcess() on Windows or
    // SIGKILL on Unix.
    void kill();

public:
    // Connect to this signal to set up the child process whenever it's
    // restarted.  This is connected to Process::prepareChildProcess - it runs
    // between fork() and exec() in the child process.
    //
    // Note that although the first execution in the forked child occurs during
    // enable(), it can also occur when the process is being restarted due to a
    // failure.
    Signal<> prepareChildProcess;

    // Line printed to standard output.  (Lines printed to stderr are logged by
    // ProcessRunner.)
    Signal<StringSlice> stdoutLine;

    // The process has been started.  This will eventually be followed by
    // succeeded() or failed(), unless ProcessRunner is disabled.
    // The PID given is provided by Process::pid() (int on Unix, DWORD on
    // Windows).
    Signal<int> started;

    // The process succeeded - it has run continuously for the success time set
    // in the RestartStrategy.
    Signal<> succeeded;

    // The process failed - we received finished() unexpectedly.  (This occurs
    // even if the exit code was "successful", because we didn't expect it to
    // exit at all.)  The "failure duration" provided is the duration since the
    // last successful run failed (how long the process has been failing).
    //
    // ProcessRunner will restart the process unless it's disabled as a result
    // of this signal.
    Signal<std::chrono::milliseconds> failed;

private:
    // Name used for tracing
    std::string _name;
    // Program and arguments last passed to enabled().  These are set when
    // _enabled is set (though _arguments could be empty) and clear otherwise.
    std::string _program;
    std::vector<std::string> _arguments;
    RestartStrategy _restartStrategy;
    // Whether ProcessRunner is enabled - whether we want the process to be
    // running right now.
    bool _enabled;
    // Current state - primarily indicates whether QProcess::finished() is
    // expected or not.
    State _state;
    // When running, this is the current instance of the process.
    nullable_t<Process> _pProcess;
    // When waiting, this is the timer that will restart or clean up after the
    // process has exited.
    Timer _postExitTimer;
    // Buffers for stdout and stderr
    LineSink _stdoutSink, _stderrSink;
};

}}

#endif
