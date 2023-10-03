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

#include <common/src/common.h>
#line HEADER_FILE("processrunner.h")

#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include <common/src/linebuffer.h>
#include <QProcess>
#include <QTimer>
#include <QDeadlineTimer>

// RestartStrategy controls restart delays for ProcessRunner.
//
// ProcessRunner notifies RestartStrategy of start, stop, and failure events for
// the process.  RestartStrategy uses this information to determine delay times
// following a process failure and measure the failure duration.
//
// If a process runs for longer than successRunTime, it is considered
// "successful" - RestartStrategy resets its ongoing measurements and emits the
// processSucceeded() signal.
class RestartStrategy : public QObject
{
    Q_OBJECT

private:
    enum : std::chrono::milliseconds::rep
    {
        // Factor applied to delay for repeated failures
        BackoffFactor = 2
    };

public:
    struct Params
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

signals:
    // When the process has run for longer than _successRunTime, this signal is
    // emitted.
    void processSucceeded();

private:
    // Configured timing parameters
    const Params _params;
    // Next delay to use if the process fails again before _successDeadline
    // elapses.
    std::chrono::milliseconds _nextDelay;
    // This timer is started when the process is started; if it elapses, the
    // process has succeeded.
    QTimer _successTimer;
    // Time since the last successful run failed.
    QElapsedTimer _lastSuccessEnd;
};

// A QProcess subclass that allows the setting of uid and gid
class UidGidProcess : public QProcess
{
    Q_OBJECT

public:
    // Set the user for the process (eg "root")
    void setUser(const QString &user) { _desiredUser = user; }

    // Set the group for the process (e.g "piavpn")
    void setGroup(const QString &group) { _desiredGroup = group; }
protected:

     // This hook method is called immediately before the exec()
     // allowing us to set the uid/gid of the process
     void setupChildProcess() override;

     QString _desiredUser{};
     QString _desiredGroup{};
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
// Use QObject::setObjectName() to name the object; its name is traced with any
// diagnostics.
class ProcessRunner : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        // Idle - Process is in the expected state, waiting for something to
        // happen.
        // _process is set when enabled; clear when disabled.
        // (If ProcessRunner is enabled, the process is running.  If it is
        // disabled, the process is not running.  Either way,
        // finished()/errorOccurred() is unexpected.)
        Idle,
        // Exiting - Process is being told to exit, QProcess::finished() is
        // expected.
        // _process is always set in this state.
        // ProcessRunner could be enabled or disabled at this point (enabled
        // occurs if it is disabled, then re-enabled while the process is still
        // exiting).
        Exiting,
        // Waiting - Process has exited and we're waiting to either clean up or
        // restart it.
        // _process is always valid in this state; it's the last process that
        // was started (we can't destroy it immediately).
        // _restartTimer is running in this state.
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
    // with _process empty.
    void startProcess();

    // stdout data is ready to read.
    void stdoutReadyRead();

    // stderr data is ready to read.
    void stderrReadyRead();

    // Handle a fatal error from either processErrorOccurred() or
    // processFinished()
    void handleProcessEnded();

    // Process encountered an error.  Some errors are fatal and indicate that
    // finished() won't be emitted; others are not.
    void processErrorOccurred(QProcess::ProcessError error);

    // Process has finished.
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // Post-exit timer has elapsed
    void postExitTimerElapsed();

public:
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
    bool enable(QString program, QStringList arguments);
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
    // This uses QProcess::terminate(), which sends WM_CLOSE on Windows or
    // SIGTERM on Unix.
    bool terminate();

    // Kill the process if it is running - if ProcessRunner is enabled, the
    // process will be restarted (this is treated as an unexpected failure).
    // Usually used when an error is detected that may not cause the process to
    // exit.
    //
    // This uses QProcess::kill(), which uses TerminateProcess() on Windows or
    // SIGKILL on Unix.
    void kill();

    // Override to do additional initialization of the process before it's
    // started.
    virtual void setupProcess(UidGidProcess &process);

signals:
    // Line printed to standard output.
    void stdoutLine(const QByteArray &line);

    // The process has been started.  This will eventually be followed by
    // succeeded() or failed(), unless ProcessRunner is disabled.
    // The PID given is provided by QProcess::processId() (int on Unix, DWORD on
    // Windows).
    void started(qint64 pid);

    // The process succeeded - it has run continuously for the success time set
    // in the RestartStrategy.
    void succeeded();

    // The process failed - we received finished() unexpectedly.  (This occurs
    // even if the exit code was "successful", because we didn't expect it to
    // exit at all.)  The "failure duration" provided is the duration since the
    // last successful run failed (how long the process has been failing).
    //
    // ProcessRunner will restart the process unless it's disabled as a result
    // of this signal.
    void failed(std::chrono::milliseconds failureDuration);

private:
    // Program and arguments last passed to enabled().  These are set when
    // _enabled is set (though _arguments could be empty) and clear otherwise.
    QString _program;
    QStringList _arguments;
    RestartStrategy _restartStrategy;
    // Whether ProcessRunner is enabled - whether we want the process to be
    // running right now.
    bool _enabled;
    // Current state - primarily indicates whether QProcess::finished() is
    // expected or not.
    State _state;
    // When running, this is the current instance of the process.
    nullable_t<UidGidProcess> _process;
    // When waiting, this is the timer that will restart or clean up after the
    // process has exited.
    QTimer _postExitTimer;
    // Buffers for stdout and stderr
    LineBuffer _stdoutBuf, _stderrBuf;
};

#endif
