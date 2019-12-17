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

#ifndef TASKS_H
#define TASKS_H
#pragma once

#include "common.h"
#include "util.h"

extern bool g_rebootAfterInstall;
extern bool g_rebootBeforeInstall;

#ifdef INSTALLER
// Record an action for the uninstall.dat file
void recordUninstallAction(std::string type, std::wstring path);
// Record the installed size
void recordInstallationSize(size_t size);
#else
#define recordUninstallAction(t,p) ((void)0)
#define recordInstallationSize(s) ((void)0)
#endif

// Flags for raising errors; the first three values are also possible
// return values for InstallerError::raise(), for handling errors.
enum ErrorType
{
    Abort = 0x0,        // Throw an exception to abort the installation (default)
    Retry = 0x1,        // Retry the failed action
    Ignore = 0x2,       // Ignore and proceed anyway

    ShouldIgnore = 0x4, // Hint for silent/passive installs (default is to abort)
    Silent = 0x40,      // Forced silence (just throws)
    NoThrow = 0x80,     // Return Abort instead of throwing
};
static inline constexpr ErrorType operator|(ErrorType a, ErrorType b) { return (ErrorType)((int)a | (int)b); }
static inline constexpr ErrorType& operator|=(ErrorType& a, ErrorType b) { return a = (ErrorType)((int)a | (int)b); }

// Exception thrown to signal errors to abort the installation process.
// Normally you would use InstallerError::raise() to display an error to
// the user before throwing and triggering the rollback process.
class InstallerError : public std::exception
{
    UIString _str;
private:
    InstallerError(UIString str) : _str(std::move(str)) {}
public:
    const UIString &description() const { return _str; }

    // Raise an error to the user's attention before throwing.
    static ErrorType raise(ErrorType type, UIString str);
    // Raise a critical error (doesn't return).
    __declspec(noreturn) static void abort(UIString str);
};

// Retry an expression until it succeeds (evaluates to true), giving the user a choice to abort on each failure. Returns the evaluated value.
#define retryLoop(expr, errorMessage) (([&] { for (;;) { if (auto value = (expr)) return value; else InstallerError::raise(Abort | Retry, (errorMessage)); } })())
// Same as retryLoop but also allows the user to ignore the error. Inspect the return value for truthiness to deduce the user's choice.
#define retryIgnoreLoop(expr, errorMessage) (([&] { for (;;) { if (auto value = (expr)) return value; else if (Ignore == InstallerError::raise(Abort | Retry | Ignore, (errorMessage))) return value; } })())

// Token exception when the user wants to abort the installation
class InstallerAbort : public std::exception
{
public:
    InstallerAbort() {}
};

class ProgressListener
{
public:
    // During execution, progress refers to a ratio of progress between
    // 0.0 and 1.0, whereas timeRemaining is the estimated number of seconds
    // remaining until completion. During rollback, progress moves in reverse.
    virtual void setProgress(double progress, double timeRemaining) {}
    // Set the caption for the current task
    virtual void setCaption(UIString caption) {}
};

class Task
{
public:
    virtual ~Task() {}
    // Set the listener (required before any other functions are called).
    virtual void setListener(ProgressListener* listener) { _listener = listener; }
    // Pre-execution step (e.g. calculate time estimates).
    virtual void prepare() {}
    // Actual execution. Throws InstallerError to signal fatal errors.
    virtual void execute() {}
    // Rollback to undo changes; if an exception is thrown during the process,
    // this is called for all tasks whose execute function has been called
    // (including the one that threw the exception).
    virtual void rollback() {}
    // Query whether the task needs to be rolled back or not.
    virtual bool needsRollback() const { return true; }
    // Get the estimated execution time of the task. This estimate also serves
    // as the basis for the relative share of the total progress bar, so it
    // should preferably not change once execution has started.
    virtual double getEstimatedExecutionTime() const { return 0.0; }
    // Get the estimated rollback time of the task. This doesn't affect the
    // progress bar (which simply runs in reverse), but it does affect the
    // reported remaining time when rolling back. Can contain an initial
    // estimate but must return a constant value after execute() has run.
    virtual double getEstimatedRollbackTime() const { return getEstimatedExecutionTime(); }
protected:
    ProgressListener* _listener = nullptr;
};

// Task to simply change the caption.
class CaptionTask : public Task
{
public:
    CaptionTask() {}
    CaptionTask(UIString caption);
    virtual void execute() override;
private:
    UIString _caption;
};

#endif // TASKS_H
