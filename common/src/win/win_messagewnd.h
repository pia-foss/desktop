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
#line HEADER_FILE("win/win_messagewnd.h")

#ifndef WIN_MESSAGEWND_H
#define WIN_MESSAGEWND_H

#include <Windows.h>

// MessageWnd creates and owns a message-only window.  This is a bare-bones
// interface to create a window for use with other resources that require a
// window for notifications (which tray icons do).
//
// Derive from MessageWnd and implement proc() to handle any messages of
// interest.  Call MessageWnd::proc() for any messages that your class doesn't
// process (calls ::DefWindowProc()).
class COMMON_EXPORT MessageWnd
{
private:
    // The MessageWnd window class - registered when the first MessageWnd is
    // created.
    static ATOM _wndClass;
    // Window proc used for MessageWnd windows - basically just a trampoline
    // that gets the MessageWnd address from the window extra data and hands
    // off to proc().
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                       LPARAM lParam);

    // Register the window class if it hasn't already been registered.
    static void registerWndClass();

    enum : int
    {
        // Offset of the MessageWnd* in the window's extra data.
        ObjPtrOffset = 0,
        // Total size of extra data
        ExtraDataBytes = sizeof(MessageWnd*),
    };

public:
    enum class WindowType
    {
        MessageOnly,
        Invisible
    };

public:
    // Create a MessageWnd.  The default is for the underlying window handle to
    // be a message-only window.  The underlying window can be a real (but
    // invisible) window instead by specifying WindowType::Invisible (which can
    // be used to receive broadcast messages).
    explicit MessageWnd(WindowType type = WindowType::MessageOnly);
    virtual ~MessageWnd();

private:
    MessageWnd(const MessageWnd &) = delete;
    MessageWnd &operator=(const MessageWnd &) = delete;

protected:
    // Handle a message for this window.  uMsg, wParam, and lParam are the
    // typical message parameters defined for window procedures and the
    // various messges.
    //
    // The default calls ::DefWindowProc().  Override to handle custom messages;
    // always call the base implementation for any unhandled messages (and
    // return its result).
    //
    // This isn't called for the initial messages sent by ::CreateWindowEx().
    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    // Get the actual window handle.
    HWND getHwnd() const {return _window;}

private:
    // The actual window handle.  MessageWnd always owns a valid window.
    HWND _window;
};

#endif
