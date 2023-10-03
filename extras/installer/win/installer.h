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

#ifndef INSTALLER_H
#define INSTALLER_H
#pragma once

#include "common.h"
#include "util.h"
#include "tasks.h"

#include <Shobjidl.h>

class AutoLock
{
    CRITICAL_SECTION* _mutex;
public:
    AutoLock(CRITICAL_SECTION* mutex) : _mutex(mutex) { EnterCriticalSection(_mutex); }
    AutoLock(CRITICAL_SECTION& mutex) : _mutex(&mutex) { EnterCriticalSection(_mutex); }
    AutoLock(const AutoLock&) = delete;
    AutoLock(AutoLock&&) = delete;
    void unlock() { LeaveCriticalSection(std::exchange(_mutex, nullptr)); }
    ~AutoLock() { if (_mutex) LeaveCriticalSection(_mutex); }
};

template<typename Handle>
class AutoGDIHandle
{
    Handle _handle;
public:
    AutoGDIHandle(Handle handle = NULL) : _handle(handle) {}
    AutoGDIHandle(const AutoGDIHandle&) = delete;
    AutoGDIHandle(AutoGDIHandle&& move) : _handle(std::exchange(move._handle, NULL)) {}
    ~AutoGDIHandle() { if (_handle) DeleteObject((HGDIOBJ)_handle); }
    AutoGDIHandle& operator=(AutoGDIHandle&& move) { if (_handle) DeleteObject((HGDIOBJ)_handle); _handle = std::exchange(move._handle, NULL); return *this; }
    void set(Handle handle) { if (_handle) DeleteObject((HGDIOBJ)_handle); _handle = handle; }
    Handle detach() { return std::exchange(_handle, NULL); }
    operator Handle() const { return _handle; }
};

template<typename Value>
class any_cast_t
{
    Value value;
    template<typename From>
    any_cast_t(From&& from) : value(std::forward<From>(from)) {}
    template<typename From>
    friend any_cast_t<From> any_cast(From&&);
public:
    template<typename To> operator To() { return (To)std::move(value); }
};
template<typename From>
static inline any_cast_t<From> any_cast(From&& value) { return any_cast_t<From>(std::forward<From>(value)); }


class Installer : public ProgressListener
{
public:
    Installer();
    ~Installer();

    int run();

    enum State
    {
        Initializing,
        Preparing,
        ReadyToInstall,
        Installing,
        Aborting,
        Committing,
        RollingBack,
        Done,
        Exiting, // Window closing
    };
    enum Result
    {
        NotSet,
        Success,
        Aborted,
        Error,
    };
    State getState();
    Result getResult();
    State setState(State state, Result result = NotSet);

    virtual void setProgress(double progress, double timeRemaining) override;
    virtual void setCaption(UIString caption) override;

    void setError(UIString description);
    void warnCorruptInstallation();
    int messageBox(UIString text, UIString caption, UIString msgSuffix, UINT type, int silentResult = 0);

    // Called from main thread
    void install();
    void abort();
    void close();

    // Called from worker thread
    void checkAbort(); // throws InstallerAbort
    State waitForNewState(State minimumState);

private:
    void loadResources();
    void createWindow();

    static DWORD WINAPI staticWorkerThreadMain(LPVOID self);
    DWORD workerThreadMain();

    static LRESULT CALLBACK staticWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT windowProc(UINT msg, WPARAM wParam, LPARAM lParam);

    State setStateAndReleaseMutex(State state, Result result = NotSet);
    void updateTaskbarProgress();

    RECT getNonClientButtonRect(int button);
    void doPaint();
    void drawText(HDC dc, const RECT& rect, COLORREF color, DWORD flags, utf16ptr text, int spacing = 1);

private:
    // Access from main thread only (no synchronization)
    // _hWnd may be read from the installer thread to post messages back to the
    // main thread (only).
    HANDLE _workerThread = NULL;
    HWND _hWnd = NULL;
    HWND _hWndButton = NULL;
    UINT _taskbarButtonCreatedMsg = 0;
    ITaskbarList3* _taskbarList = nullptr;

    // Mutexes for thread safety
    CRITICAL_SECTION _drawMutex; // GUI draw state
    CRITICAL_SECTION _stateMutex; // Guards installation state
    HANDLE _stateEvent; // Signals state changes to thread

    // Guarded by _drawMutex
    UIString _caption; // Description
    double _progress = -1.0; // Progress bar value between 0.0 and 1.0 (or negative to hide)
    double _timeRemaining = -1.0; // Remaining time estimate (or negative to hide)
    UIString _error;

    // Guarded by _stateMutex
    State _state = Initializing;
    Result _result = NotSet;
    UINT _currentMessageBoxType = 0;

    int _capturedNonClientButton = HTNOWHERE;
    bool _hoveredNonClientButton = false; // Is the mouse over the currently captured control?
    DWORD _spinnerTickCount = 0; // Time offset for when the spinner started showing

    // Resources
    DWORD _workerThreadId = 0;
    ATOM _windowClassAtom = NULL;
    bool _uiMirror = false;
    AutoGDIHandle<HICON> _appIcon;
    AutoGDIHandle<HICON> _appIconSmall;
    AutoGDIHandle<HBRUSH> _backgroundBrush;
    AutoGDIHandle<HBRUSH> _progressBackgroundBrush;
    AutoGDIHandle<HBRUSH> _progressForegroundBrush;
    AutoGDIHandle<HBRUSH> _spinnerActiveBrush;
    AutoGDIHandle<HBRUSH> _spinnerInactiveBrush;
    AutoGDIHandle<HPEN> _buttonBorderPen;
    AutoGDIHandle<HBITMAP> _logoBitmap;
    AutoGDIHandle<HBITMAP> _minimizeBitmap;
    AutoGDIHandle<HBITMAP> _closeBitmap;
    AutoGDIHandle<HFONT> _mainFont;
};

extern Installer* g_installer;

#endif // INSTALLER_H
