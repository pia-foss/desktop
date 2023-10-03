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
#line SOURCE_FILE("win/win_subclasswnd.cpp")

#include "win_subclasswnd.h"

LRESULT CALLBACK SubclassWnd::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam)
{
    auto itSubclassedWindow = _subclassedWindows.find(hwnd);

    // Class invariant - windowProc is only assigned to a window
    // while it's subclassed by SubclassWnd
    Q_ASSERT(itSubclassedWindow != _subclassedWindows.end());

    SubclassWnd *pSubclass = itSubclassedWindow->first;
    WNDPROC pOrigWndProc = itSubclassedWindow->second;

    // Is the window being destroyed?  Un-subclass if it is.
    if(uMsg == WM_NCDESTROY)
    {
        LONG_PTR pOurProc = ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                                                reinterpret_cast<LONG_PTR>(pOrigWndProc));
        // If the window procedure wasn't SubclassWnd's, it means that
        // un-subclassing did not occur in the correct order.
        Q_ASSERT(pOurProc == reinterpret_cast<LONG_PTR>(&SubclassWnd::windowProc));

        _subclassedWindows.erase(itSubclassedWindow);
        // We don't give the SubclassWnd a chance to handle WM_NCDESTROY.
        // Clear it's HWND if it's still around though, since it is no longer
        // subclassing anything.
        if(pSubclass)
            pSubclass->_hWnd = nullptr;
        return ::CallWindowProcW(pOrigWndProc, hwnd, uMsg, wParam, lParam);
    }

    // Does the SubclassWnd still exist?
    if(pSubclass)
        return pSubclass->proc(uMsg, wParam, lParam);

    // The SubclassWnd no longer exists, go directly to the original proc.
    return ::CallWindowProcW(pOrigWndProc, hwnd, uMsg, wParam, lParam);
}

// Unlike MessageWnd, we can't store the SubclassWnd pointer in the window extra
// data, because we don't control the window's class to allocate any extra data.
// Identify SubclassWnd objects using this map.
//
// Additionally, libangle (used by Qt when using hardware accelerated rendering)
// also will subclass the window when the rendering context is created.  We have
// to ensure that un-subclassing occurs in the reverse order of subclassing, or
// we won't restore each others' window procedures correctly.
//
// That means we have to un-subclass when the underlying window is destroyed,
// which is not necessarily when the SubclassWnd is destroyed.
//
// To do that, we store the original window procedure pointer in this map, not
// in the SubclassWnd.  If the SubclassWnd goes away, its pointer here is nulled
// out, but the map entry remains until the underlying window is destroyed.  At
// that point, windowProc() un-subclasses the window and removes the entry.
QMap<HWND, QPair<SubclassWnd*, WNDPROC>> SubclassWnd::_subclassedWindows;

SubclassWnd::SubclassWnd(HWND hWnd)
    : _hWnd{hWnd}
{
    Q_ASSERT(hWnd);  //Guaranteed by caller
    // Can't subclass the same window more than once.
    Q_ASSERT(_subclassedWindows.count(hWnd) == 0);

    // Apply the new window procedure and store the old one.
    LONG_PTR pNewProc = reinterpret_cast<LONG_PTR>(&SubclassWnd::windowProc);
    LONG_PTR pOldProc = ::SetWindowLongPtrW(_hWnd, GWLP_WNDPROC, pNewProc);
    _subclassedWindows.insert(_hWnd, {this, reinterpret_cast<WNDPROC>(pOldProc)});
}

SubclassWnd::~SubclassWnd()
{
    // If this window is still subclassed, just wipe out its SubclassWnd
    // pointer.  We won't intercept any more messages for this window.
    if(_hWnd)
    {
        auto itSubclassedWnd = _subclassedWindows.find(_hWnd);
        // Class invariant - if _hWnd is set, we're present in
        // _subclassedWindows
        Q_ASSERT(itSubclassedWnd != _subclassedWindows.end());
        // Class invariant - we are the SubclassWnd subclassing this window,
        // there can't be more than one
        Q_ASSERT(itSubclassedWnd->first == this);
        itSubclassedWnd->first = nullptr;
    }
}

LRESULT SubclassWnd::proc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Guarantee of windowProc(); we're only called if we're subclassing this window
    Q_ASSERT(_hWnd);
    auto itSubclassedWnd = _subclassedWindows.find(_hWnd);
    // Guarantee of windowProc(), there is a valid entry for this _hWnd (that's
    // how it found us).
    Q_ASSERT(itSubclassedWnd != _subclassedWindows.end());
    // Invariant - can't have a nullptr window proc
    Q_ASSERT(itSubclassedWnd->second);
    return ::CallWindowProcW(itSubclassedWnd->second, _hWnd, uMsg, wParam,
                             lParam);
}
