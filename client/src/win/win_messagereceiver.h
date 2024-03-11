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
#line HEADER_FILE("win/win_messagereceiver.h")

#ifndef WIN_MESSAGERECEIVER_H
#define WIN_MESSAGERECEIVER_H

#include <common/src/win/win_messagewnd.h>

// - Detects the exit message broadcast by the uninstaller and quits the client.
//   Message-only windows don't receive broadcasts, so this creates an invisible
//   top-level window.
//
// - Detects messages to show the dashboard
class MessageReceiver : public MessageWnd
{
public:
    MessageReceiver();
    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    static void broadcastMessage(const LPCWSTR &message);

private:
    UINT _exitMsgCode;
    UINT _showDashboardMsgCode;
};

#endif
