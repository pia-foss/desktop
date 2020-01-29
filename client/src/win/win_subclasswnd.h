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
#line HEADER_FILE("win/win_subclass.h")

#ifndef WIN_SUBCLASS_H
#define WIN_SUBCLASS_H

#include <windows.h>

// SubclassWnd subclasses a window to intercept window messages intended for it.
// This is mostly necessary with QtQuick.Window objects, since the type that
// implements them isn't actually a QQuickWindow, and we can't derive from the
// type in order to override QWindow::winEvent().
//
// Instead, SubclassWnd overrides the window's window procedure.
class SubclassWnd
{
private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                       LPARAM lParam);

    static QMap<HWND, QPair<SubclassWnd*, WNDPROC>> _subclassedWindows;

public:
    // Create SubclassWnd with a native window handle to subclass.
    SubclassWnd(HWND hWnd);
    virtual ~SubclassWnd();

private:
    SubclassWnd(const SubclassWnd &) = delete;
    SubclassWnd &operator=(const SubclassWnd &) = delete;

protected:
    // Handle a message for this window.
    // Call the base class's implementation for any messages that are not
    // handled; it calls the original window procedure.
    // If the underlying window might be destroyed before SubclassWnd is, you
    // *must* forward the WM_NCDESTROY message to SubclassWnd::proc().
    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    HWND getHwnd() const {return _hWnd;}

private:
    HWND _hWnd;
};

#endif
