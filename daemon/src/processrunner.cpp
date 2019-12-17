// Copyright (c) 2019 London Trust Media Incorporated
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
#line SOURCE_FILE("processrunner.cpp")

#include "processrunner.h"

// These headers are used by UidGidProcess::set{Group,User}
#ifdef Q_OS_UNIX
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#endif

RestartStrategy::RestartStrategy(Params params)
    : _params{std::move(params)}
{
    _successTimer.setSingleShot(true);
    _successTimer.setInterval(msec32(_params._successRunTime));
    connect(&_successTimer, &QTimer::timeout, this,
        [this]()
        {
            resetFailures();
            emit processSucceeded();
        });

    resetFailures();
}

void RestartStrategy::resetFailures()
{
    _nextDelay = _params._initialDelay;
    // Start measuring the failure time from the next failure
    _lastSuccessEnd.invalidate();
}

void RestartStrategy::processStarting()
{
    _successTimer.start();

    // If the process was stopped before, start tracking the failure duration.
    // If it never starts successfully, the duration is measured from the
    // initial start.
    if(!_lastSuccessEnd.isValid())
        _lastSuccessEnd.start();
}

std::chrono::milliseconds RestartStrategy::processFailed(std::chrono::milliseconds &failureDuration)
{
    // Is this the first failure since a successful run?
    if(!_lastSuccessEnd.isValid())
        _lastSuccessEnd.start();    // Start measuring from this failure

    // Otherwise, keep the current _lastSuccessEnd

    auto thisDelay = _nextDelay;
    _nextDelay *= BackoffFactor;
    if(_nextDelay > _params._maxDelay)
        _nextDelay = _params._maxDelay;

    // Not guaranteed to be 0 for the first failure; that's fine.
    failureDuration = std::chrono::milliseconds(_lastSuccessEnd.elapsed());
    return thisDelay;
}

void UidGidProcess::setupChildProcess()
{
#ifdef Q_OS_UNIX
    // Must set group first (otherwise we might not have privs to set group later if setUser is successful)
    if(!_desiredGroup.isEmpty())
    {
        struct group* gr = getgrnam(qPrintable(_desiredGroup));
        if(!gr)
        {
            qWarning("Failed to set group to %s (%d: %s)", qPrintable(_desiredGroup), errno, qPrintable(qt_error_string(errno)));
        }
        else if(setegid(gr->gr_gid) == -1 && setgid(gr->gr_gid) == -1)
        {
            qWarning("Failed to set group id to %d (%d: %s)", gr->gr_gid, errno, qPrintable(qt_error_string(errno)));
        }
        else
        {
            qInfo().noquote() << "Set group of process" << program() << "to:" << _desiredGroup << "with gid:" << gr->gr_gid;
        }
    }

    if(!_desiredUser.isEmpty())
    {
        struct passwd* pw = getpwnam(qPrintable(_desiredUser));
        if(!pw)
        {
            qWarning("Failed to set user to %s (%d: %s)", qPrintable(_desiredUser), errno, qPrintable(qt_error_string(errno)));
        }
        else if(seteuid(pw->pw_uid) == -1 && setuid(pw->pw_uid) == -1)
        {
            qWarning("Failed to set user id to %d (%d: %s)", pw->pw_uid, errno, qPrintable(qt_error_string(errno)));
        }
        else
        {
            qInfo().noquote() << "Set user of process" << program() << "to:" << _desiredUser << "with uid:" << pw->pw_uid;
        }
    }
#endif
}

ProcessRunner::ProcessRunner(RestartStrategy::Params restartParams)
    : _restartStrategy{std::move(restartParams)},
      _enabled{false}, _state{State::Idle}
{
    connect(&_restartStrategy, &RestartStrategy::processSucceeded, this, [this]()
    {
        qInfo() << "Process" << objectName() << "startup succeeded";
        emit succeeded();
    });

    _postExitTimer.setSingleShot(true);
    connect(&_postExitTimer, &QTimer::timeout, this,
            &ProcessRunner::postExitTimerElapsed);

    // stdout is forwarded
    connect(&_stdoutBuf, &LineBuffer::lineComplete, this,
            &ProcessRunner::stdoutLine);
    // stderr is just logged at warning level
    connect(&_stderrBuf, &LineBuffer::lineComplete, this,
        [this](const QByteArray &line)
        {
            qWarning() << objectName() << "- stderr:" << line;
        });
}

ProcessRunner::~ProcessRunner()
{
    // QProcess already kills the process and waits for it to exit in its
    // destructor.
    // Since it can emit signals from its destructor though, we have to
    // explicitly disconnect from it.
    if(_process)
        _process->disconnect(this);
}

void ProcessRunner::startProcess()
{
    // Preconditions checked by callers:
    Q_ASSERT(_state == State::Idle);
    Q_ASSERT(_enabled);
    Q_ASSERT(!_process);

    _stderrBuf.reset();
    _process.emplace();

    connect(_process.ptr(), &QProcess::errorOccurred, this,
            &ProcessRunner::processErrorOccurred);
    connect(_process.ptr(),
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProcessRunner::processFinished);
    connect(_process.ptr(), &QProcess::readyReadStandardError, this,
            &ProcessRunner::stderrReadyRead);
    connect(_process.ptr(), &QProcess::readyReadStandardOutput, this,
            &ProcessRunner::stdoutReadyRead);

    _process->setProgram(_program);
    _process->setArguments(_arguments);

    setupProcess(*_process);

    _process->start();

    _restartStrategy.processStarting();

    emit started();
}

void ProcessRunner::stdoutReadyRead()
{
    Q_ASSERT(_process); // Valid because signal is connected to _process
    _stdoutBuf.append(_process->readAllStandardOutput());
}

void ProcessRunner::stderrReadyRead()
{
    Q_ASSERT(_process); // Valid because signal is connected to _process
    _stderrBuf.append(_process->readAllStandardError());
}

void ProcessRunner::handleProcessEnded()
{
    // If there's anything left in the stderr buffer, print it, the process
    // didn't terminate the line being printed.
    QByteArray partialStderrLine = _stderrBuf.reset();
    if(!partialStderrLine.isEmpty())
    {
        qWarning() << objectName() << "- stderr:" << partialStderrLine;
        partialStderrLine.clear();
    }

    // The process has exited.  Did we expect it to terminate?
    bool unexpected = false;
    switch(_state)
    {
    case State::Waiting:
        // Shouldn't normally happen, probably means that we received both a
        // "fatal" error signal and a finished signal.  Nothing to do since
        // we've already started the post-exit timer.
        qInfo() << objectName() << "- Ignoring additional process exit event while already waiting";
        return;
    case State::Exiting:
        // Yes, this was expected.  Return to Idle and check _enabled below.
        qInfo() << objectName() << "- Process finished exiting";
        break;
    case State::Idle:
        // No, this wasn't expected.
        qWarning() << objectName() << "- Process exited unexpectedly";
        unexpected = true;
        break;
    }

    // Go to the Waiting state and start the post-exit timer.  We can't destroy
    // the QProcess or restart it now; QProcess's signals aren't reentrant (see
    // QProcessPrivate::_q_processDied()).
    _state = State::Waiting;
    if(unexpected)
    {
        std::chrono::milliseconds failureDuration;
        std::chrono::milliseconds retryDelay = _restartStrategy.processFailed(failureDuration);

        qWarning() << objectName() << "- Has been failing for"
            << traceMsec(failureDuration) << "- restart after"
            << traceMsec(retryDelay);

        // This was unexpected, so wait briefly before restarting
        _postExitTimer.start(msec32(retryDelay));
        // This was unexpected so emit a failure signal.  (Do this last after
        // all state changes are done.)
        emit failed(failureDuration);
    }
    else
    {
        // Since this was expected, forget all past failures
        _restartStrategy.resetFailures();
        // If we need to restart hnsd, do the next steps immediately (this can
        // happen if we disconnect and quickly reconnect while hnsd is
        // terminating).
        _postExitTimer.start(0);
    }
}

void ProcessRunner::processErrorOccurred(QProcess::ProcessError error)
{
    Q_ASSERT(_process); // Valid because signal is connected to _process

    QProcess::ProcessState procState = _process->state();
    qWarning() << objectName() << "- Process signaled error:" << error
        << "in state" << procState;

    // Some errors, like FailedToStart, indicate that the process did not start,
    // so the finished() signal won't be emitted.  A few errors, like Crashed,
    // are used both ways.
    //
    // QProcess ultimately emits finished() if it had reached the Running state,
    // so the most reliable way to handle this is to check that state.  If it's
    // still in the Running state, we'll receive finished(), otherwise we won't.
    if(procState != QProcess::ProcessState::Running)
        handleProcessEnded();
}

void ProcessRunner::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qInfo() << objectName() << "- Process exited with code" << exitCode
        << "and status" << exitStatus;
    handleProcessEnded();
}

void ProcessRunner::postExitTimerElapsed()
{
    Q_ASSERT(_process); // Valid in this state
    Q_ASSERT(_state == State::Waiting); // Timer only runs in this state

    _state = State::Idle;
    _process.clear();
    if(_enabled)
    {
        qInfo() << objectName() << "- Restarting process now";
        startProcess();
    }
    else
    {
        qInfo() << objectName() << "- Cleaned up after process exit, now idle";
    }
}

bool ProcessRunner::enable(QString program, QStringList arguments)
{
    if(_enabled)
    {
        if(_program == program && _arguments == arguments)
        {
            qInfo() << "Already enabled" << objectName()
                << ", program/args have not changed, nothing to do.";
            return false; // Already enabled, command/args have not changed, nothing to do
        }

        // This could happen occasionally but shouldn't happen a lot.  Trace it
        // in case it indicates an incorrect state transition in VPNConnection.
        qInfo() << "Already enabled" << objectName()
            << ", but program/arguments have changed, restart process."
            << "Old:" << _program << _arguments << "- New:" << program
            << arguments;
        // Disable to handle any possible enabled state, kill the process if
        // needed.
        disable();
    }

    qInfo() << "Enabling" << objectName() << "with" << program << arguments;

    _enabled = true;
    _program = std::move(program);
    _arguments = std::move(arguments);

    switch(_state)
    {
    case State::Idle:
        Q_ASSERT(!_process); // Class invariant, clear in Idle state with _enabled==false
        // Start it now.
        startProcess();
        break;
    case State::Exiting:
        // Nothing to do, process is already running and exiting, we'll restart
        // it after we receive the expected finished() signal.
        break;
    case State::Waiting:
        // Nothing to do, still in a post-exit delay.
        Q_ASSERT(_postExitTimer.isActive());    // Running in this state
        // The timer interval is always 0 ms at this point.  If we had set a
        // nonzero delay after it exited unexpectedly, an intervening call to
        // disable() cleared the remaining delay.
        Q_ASSERT(_postExitTimer.interval() == 0);
        break;
    }

    return true;
}

void ProcessRunner::disable()
{
    if(!_enabled)
        return; // Nothing to do

    qInfo() << "Disabling" << objectName();

    _enabled = false;
    _program = QString{};
    _arguments = QStringList{};

    switch(_state)
    {
    case State::Idle:
        Q_ASSERT(_process); // Class invariant, valid in Idle with _enabled==true
        // Kill the process.
        _state = State::Exiting;
        _process->kill();
        break;
    case State::Exiting:
        // Nothing to do, process is already exiting.
        break;
    case State::Waiting:
        // Since we're just stopping now, not restarting, skip any remaining
        // delay and clean up now.
        // This also ensures that if the ProcessRunner is disabled and
        // re-enabled during a post-exit delay, that we skip the delay and
        // restart immediately.
        Q_ASSERT(_postExitTimer.isActive());    // Running in this state
        _postExitTimer.start(0);
        // We might have been in this delay due to a prior failure, which
        // RestartStrategy is still tracking.  Forget all prior failures; the
        // next run will be a fresh start.
        _restartStrategy.resetFailures();
        break;
    }
}

void ProcessRunner::kill()
{
    if(_process)
    {
        qInfo() << objectName() << "- killing process";
        // Just kill the process, the resulting QProcess state change will be
        // treated as unexpected.
        _process->kill();
    }
    else
    {
        qInfo() << objectName() << "- process not running, nothing to kill";
    }
}

void ProcessRunner::setupProcess(UidGidProcess &)
{
    // Default does nothing
}
