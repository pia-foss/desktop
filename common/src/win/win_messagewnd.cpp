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
#line SOURCE_FILE("win/win_messagewnd.cpp")

#include "win_messagewnd.h"

#pragma comment(lib, "User32.lib")

ATOM MessageWnd::_wndClass{0};
LRESULT CALLBACK MessageWnd::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                        LPARAM lParam)
{
    MessageWnd *pMessageWnd{nullptr};

    switch(uMsg)
    {
        // These two messages occur in ::CreateWindowEx() before WM_CREATE, so
        // we don't have the MessageWnd yet.  That's OK, these messages can't be
        // intercepted; call ::DefWindowProc() directly below.
        case WM_NCCREATE:
        case WM_NCCALCSIZE:
            break;
        // Grab the MessageWnd* from the creation data and store it.
        case WM_CREATE:
        {
            CREATESTRUCTW *pCreateData = reinterpret_cast<CREATESTRUCTW*>(lParam);
            Q_ASSERT(pCreateData);  //WinApi guarantee
            pMessageWnd = reinterpret_cast<MessageWnd*>(pCreateData->lpCreateParams);
            ::SetWindowLongPtrW(hwnd, ObjPtrOffset, reinterpret_cast<LONG_PTR>(pMessageWnd));
            break;
        }
        default:
        {
            // Get the MessageWnd* from the extra data.
            LONG_PTR objPtrVal = GetWindowLongPtrW(hwnd, ObjPtrOffset);
            pMessageWnd = reinterpret_cast<MessageWnd*>(objPtrVal);
            // This should be valid by now.
            //Q_ASSERT(pMessageWnd);
            break;
        }
    }

    if(pMessageWnd)
        return pMessageWnd->proc(uMsg, wParam, lParam);
    else
        return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void MessageWnd::registerWndClass()
{
    if(_wndClass)
        return; //Nothing to do, already registered

    WNDCLASSEXW wndclassex{};
    wndclassex.cbSize = sizeof(WNDCLASSEXW);
    wndclassex.style = 0; //Messages only; don't care about these flags
    wndclassex.lpfnWndProc = &MessageWnd::windowProc;
    wndclassex.cbClsExtra = 0;
    wndclassex.cbWndExtra = ExtraDataBytes;
    wndclassex.hInstance = ::GetModuleHandleW(nullptr);
    wndclassex.hIcon = nullptr;
    wndclassex.hCursor = nullptr;
    wndclassex.hbrBackground = nullptr;
    wndclassex.lpszMenuName = nullptr;
    wndclassex.lpszClassName = L"wndclass_messagewnd";
    wndclassex.hIconSm = nullptr;

    _wndClass = ::RegisterClassExW(&wndclassex);

    if(!_wndClass)
    {
        // Couldn't register the window class for some reason - we're hosed.
        CHECK_THROW("Registering window class for tray icon message window");
    }
}

MessageWnd::MessageWnd(WindowType type)
    : _window{nullptr}
{
    registerWndClass();
    HWND parent = (type == WindowType::MessageOnly) ? HWND_MESSAGE : nullptr;
    _window = ::CreateWindowExW(0, reinterpret_cast<LPCTSTR>(_wndClass), L"",
                                0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                CW_USEDEFAULT, parent, nullptr,
                                ::GetModuleHandle(nullptr),
                                reinterpret_cast<LPVOID>(this));
    if(!_window)
        CHECK_THROW("Creating tray icon message window");
}

MessageWnd::~MessageWnd()
{
    ::DestroyWindow(getHwnd());
}

LRESULT MessageWnd::proc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return ::DefWindowProc(getHwnd(), uMsg, wParam, lParam);
}
