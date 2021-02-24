// Copyright (c) 2021 Private Internet Access, Inc.
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

#ifndef LINUX_LAUNCHER_CMDS_H
#define LINUX_LAUNCHER_CMDS_H

// The support tool sends single-byte commands to the launcher on Linux to
// invoke the client.  (The commands are single bytes to avoid needing any
// framing/buffering on the pipe.)
enum LinuxLauncherCmds : unsigned char
{
    // Signal the launcher to start the client, either normally or with
    // "--safe-mode".  The launcher expects the support tool to exit after this;
    // it doesn't track or reap the client process.
    StartClientNormal,
    StartClientSafe
};

#endif
