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

#include "processrunner.h"
#ifndef KAPPS_CORE_OS_WINDOWS

#include "logger.h"

namespace kapps { namespace core {

RestartStrategy::RestartStrategy(Params params)
    : _params{std::move(params)}
{
    _successTimer.elapsed = [this]()
    {
        resetFailures();
        processSucceeded();
    };

    resetFailures();
}

void RestartStrategy::resetFailures()
{
    _nextDelay = _params._initialDelay;
    // Start measuring the failure time from the next failure
    _lastSuccessEnd.reset();
}

void RestartStrategy::processStarting()
{
    _successTimer.set(_params._successRunTime, true);

    // If the process was stopped before, start tracking the failure duration.
    // If it never starts successfully, the duration is measured from the
    // initial start.
    if(!_lastSuccessEnd)
        _lastSuccessEnd.start();
}

std::chrono::milliseconds RestartStrategy::processFailed(std::chrono::milliseconds &failureDuration)
{
    // Is this the first failure since a successful run?
    if(!_lastSuccessEnd)
        _lastSuccessEnd.start();    // Start measuring from this failure

    // Otherwise, keep the current _lastSuccessEnd

    auto thisDelay = _nextDelay;
    _nextDelay *= BackoffFactor;
    if(_nextDelay > _params._maxDelay)
        _nextDelay = _params._maxDelay;

    // Not guaranteed to be 0 for the first failure; that's fine.
    failureDuration = _lastSuccessEnd.elapsed();
    return thisDelay;
}

ProcessRunner::ProcessRunner(RestartStrategy::Params restartParams)
    : _restartStrategy{std::move(restartParams)},
      _enabled{false}, _state{State::Idle}
{
    _restartStrategy.processSucceeded = [this]()
    {
        KAPPS_CORE_INFO() << "Process" << name() << "startup succeeded";
        succeeded();
    };

    _postExitTimer.elapsed = [this]{postExitTimerElapsed();};

    // stdout is forwarded
    _stdoutSink.lineComplete = [this](StringSlice line){stdoutLine(line);};
    _stderrSink.lineComplete = [this](StringSlice line)
    {
        KAPPS_CORE_WARNING() << name() << "- stderr:" << line;
    };
}

ProcessRunner::~ProcessRunner()
{
    // Process kills the process and waits for it to exit in its destructor,
    // but do it ourselves to avoid a spurious warning, and to log any output
    // that might still be emitted
    disable();
    if(_pProcess)
    {
        // Use the existing output buffers, but reconnect _stdoutSink so we don't
        // emit signals during the destructor - just trace
        _stdoutSink.lineComplete = [this](StringSlice line)
        {
            KAPPS_CORE_WARNING() << name() << "- stdout while exiting:" << line;
        };
        _pProcess->terminate();
        _pProcess->waitForExit(_stdoutSink.readyFunc(), _stderrSink.readyFunc());
    }
}

void ProcessRunner::startProcess()
{
    // Preconditions checked by callers:
    assert(_state == State::Idle);
    assert(_enabled);
    assert(!_pProcess);

    _stdoutSink.reset();
    _stderrSink.reset();
    _pProcess.emplace(_program, _arguments);

    _pProcess->prepareChildProcess = [this]{prepareChildProcess();};

    _pProcess->start([this]{processFinished();}, _stdoutSink.readyFunc(),
                     _stderrSink.readyFunc());

    _restartStrategy.processStarting();

    started(_pProcess->pid());
}

void ProcessRunner::handleProcessEnded()
{
    // If there's anything left in stdout, emit it.  Usually processes do
    // terminate the last line, so log a warning - this probably indicates something
    // unexpected happened.
    auto partialStdoutLine = _stdoutSink.reset();
    if(!partialStdoutLine.empty())
    {
        KAPPS_CORE_WARNING() << name() << "- emitting partial stdout line:"
            << partialStdoutLine;
        stdoutLine(partialStdoutLine);
    }
    // If there's anything left in the stderr buffer, print it, the process
    // didn't terminate the line being printed.
    auto partialStderrLine = _stderrSink.reset();
    if(!partialStderrLine.empty())
    {
        KAPPS_CORE_WARNING() << name() << "- stderr:" << partialStderrLine.data();
        partialStderrLine.clear();
    }

    // The process has exited.  Did we expect it to terminate?
    bool unexpected = false;
    switch(_state)
    {
    case State::Waiting:
        // Shouldn't normally happen - it could indicate that Process::start()
        // failed, but somehow the exit func was invoked anyway.  Probably
        // indicaes a bug in Process, so trace at warning.
        KAPPS_CORE_WARNING() << name() << "- Ignoring additional process exit event while already waiting";
        return;
    case State::Exiting:
        // Yes, this was expected.  Return to Idle and check _enabled below.
        KAPPS_CORE_INFO() << name() << "- Process finished exiting";
        break;
    case State::Idle:
        // No, this wasn't expected.
        KAPPS_CORE_WARNING() << name() << "- Process exited unexpectedly";
        unexpected = true;
        break;
    }

    // Go to the Waiting state and start the post-exit timer.  Historially, we
    // couldn't destroy QProcess during its exit signal, but Process does support
    // this.
    _state = State::Waiting;
    if(unexpected)
    {
        std::chrono::milliseconds failureDuration;
        std::chrono::milliseconds retryDelay = _restartStrategy.processFailed(failureDuration);

        KAPPS_CORE_WARNING() << name() << "- Has been failing for"
            << traceMsec(failureDuration) << "- restart after"
            << traceMsec(retryDelay);

        // This was unexpected, so wait briefly before restarting
        _postExitTimer.set(retryDelay, true);
        // This was unexpected so emit a failure signal.  (Do this last after
        // all state changes are done.)
        failed(failureDuration);
    }
    else
    {
        // Since this was expected, forget all past failures
        _restartStrategy.resetFailures();
        // If we need to restart hnsd, do the next steps immediately (this can
        // happen if we disconnect and quickly reconnect while hnsd is
        // terminating).
        _postExitTimer.set(std::chrono::milliseconds{0}, true);
    }
}

void ProcessRunner::processFinished()
{
    assert(_pProcess);  // Invoked by Process
    KAPPS_CORE_INFO() << name() << "- Process exited with code" << _pProcess->exitCode();
    // TODO - We should indicate both exit status and code from Process
    handleProcessEnded();
}

void ProcessRunner::postExitTimerElapsed()
{
    assert(_pProcess); // Valid in this state
    assert(_state == State::Waiting); // Timer only runs in this state

    _state = State::Idle;
    _pProcess.clear();
    if(_enabled)
    {
        KAPPS_CORE_INFO() << name() << "- Restarting process now";
        startProcess();
    }
    else
    {
        KAPPS_CORE_INFO() << name() << "- Cleaned up after process exit, now idle";
    }
}

bool ProcessRunner::enable(std::string program, std::vector<std::string> arguments)
{
    if(_enabled)
    {
        if(_program == program && _arguments == arguments)
        {
            KAPPS_CORE_INFO() << "Already enabled" << name()
                << ", program/args have not changed, nothing to do.";
            return false; // Already enabled, command/args have not changed, nothing to do
        }

        // This could happen occasionally but shouldn't happen a lot.  Trace it
        // in case it indicates an incorrect state transition in VPNConnection.
        KAPPS_CORE_INFO() << "Already enabled" << name()
            << ", but program/arguments have changed, restart process."
            << "Old:" << _program << _arguments << "- New:" << program
            << arguments;
        // Disable to handle any possible enabled state, kill the process if
        // needed.
        disable();
    }

    KAPPS_CORE_INFO() << "Enabling" << name() << "with" << program << arguments;

    _enabled = true;
    _program = std::move(program);
    _arguments = std::move(arguments);

    switch(_state)
    {
    case State::Idle:
        assert(!_pProcess); // Class invariant, clear in Idle state with _enabled==false
        // Start it now.
        startProcess();
        break;
    case State::Exiting:
        // Nothing to do, process is already running and exiting, we'll restart
        // it after we receive the expected finished() signal.
        break;
    case State::Waiting:
        // Nothing to do, still in a post-exit delay.
        assert(_postExitTimer);    // Running in this state
        // The timer interval is always 0 ms at this point.  If we had set a
        // nonzero delay after it exited unexpectedly, an intervening call to
        // disable() cleared the remaining delay.
        break;
    }

    return true;
}

void ProcessRunner::disable()
{
    if(!_enabled)
        return; // Nothing to do

    KAPPS_CORE_INFO() << "Disabling" << name();

    _enabled = false;
    _program = {};
    _arguments = {};

    switch(_state)
    {
    case State::Idle:
        assert(_pProcess); // Class invariant, valid in Idle with _enabled==true
        // Kill the process.
        _state = State::Exiting;
        _pProcess->kill();
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
        assert(_postExitTimer);    // Running in this state
        _postExitTimer.set(std::chrono::milliseconds{0}, true);
        // We might have been in this delay due to a prior failure, which
        // RestartStrategy is still tracking.  Forget all prior failures; the
        // next run will be a fresh start.
        _restartStrategy.resetFailures();
        break;
    }
}

bool ProcessRunner::terminate()
{
    if(_pProcess && _pProcess->running())
    {
        KAPPS_CORE_INFO() << name() << "- terminating process";
        // Just tell it to exit, the resulting state change will be unexpected.
        _pProcess->terminate();
        return true;
    }
    else
    {
        KAPPS_CORE_INFO() << name() << "- process not running, nothing to terminate";
        return false;
    }
}

void ProcessRunner::kill()
{
    if(_pProcess && _pProcess->running())
    {
        KAPPS_CORE_INFO() << name() << "- killing process";
        // Just kill the process, the resulting QProcess state change will be
        // treated as unexpected.
        _pProcess->kill();
    }
    else
    {
        KAPPS_CORE_INFO() << name() << "- process not running, nothing to kill";
    }
}

}}

#endif
