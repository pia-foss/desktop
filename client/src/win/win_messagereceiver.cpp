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

#include <common/src/common.h>
#line SOURCE_FILE("win/win_messagereceiver.cpp")

#include "win_messagereceiver.h"
#include "../client.h"

MessageReceiver::MessageReceiver()
    : MessageWnd{WindowType::Invisible}, _exitMsgCode{0}
{
    _exitMsgCode = ::RegisterWindowMessageW(L"WM_PIA_EXIT_CLIENT");
    if(!_exitMsgCode)
    {
        qWarning() << "Unable to register exit message - error"
            << ::GetLastError();
    }

    _showDashboardMsgCode = ::RegisterWindowMessageW(L"WM_PIA_SHOW_DASHBOARD");
    if(!_showDashboardMsgCode)
    {
        qWarning() << "Unable to register show dashboard message - error"
            << ::GetLastError();
    }
}

LRESULT MessageReceiver::proc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(_exitMsgCode && uMsg == _exitMsgCode)
    {
        qInfo() << "Exiting due to uninstall";
        QCoreApplication::quit();
        return 0;
    }
    if(_showDashboardMsgCode && uMsg == _showDashboardMsgCode)
    {
        qInfo() << "Showing dashboard due to message received";
        Client::instance()->checkForURL();
        Client::instance()->openDashboard();
        return 0;
    }

    switch(uMsg)
    {
    case WM_ENDSESSION:
        // wParam indicates whether the session is actually ending.  The doc
        // states that applications should not shut down when wParam is FALSE,
        // but does not indicate when this is sent.
        //
        // lParam is a set of flags describing the shutdown; none of these
        // matter to us, we just quit.
        qInfo() << "WM_ENDSESSION:" << wParam << hex << lParam;
        if(wParam)
        {
            qInfo() << "Exiting because session is ending";
            Client::instance()->notifyExit();
            QCoreApplication::quit();
        }
        return 0;
    }

    return MessageWnd::proc(uMsg, wParam, lParam);
}
