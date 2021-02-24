#! /bin/bash

# Copyright (c) 2021 Private Internet Access, Inc.
#
# This file is part of the Private Internet Access Desktop Client.
#
# The Private Internet Access Desktop Client is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# The Private Internet Access Desktop Client is distributed in the hope that
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Private Internet Access Desktop Client.  If not, see
# <https://www.gnu.org/licenses/>.

TERMCMD=""

if [ -x /usr/bin/x-terminal-emulator ]; then
    TERMCMD=/usr/bin/x-terminal-emulator
elif [ -x /usr/bin/gnome-terminal ]; then
    TERMCMD=/usr/bin/gnome-terminal
elif [ -x /usr/bin/konsole ]; then
    TERMCMD=/usr/bin/konsole
elif [ -x /usr/bin/xterm ]; then
    TERMCMD=/usr/bin/xterm
else
    exit 3
fi

# Start the terminal with the specified command
# $1 must be an absolute path to an executable/script, it cannot be a command
# with arguments (some terminals interpret it as a command but some do not)
nohup $TERMCMD -e "$1" >/dev/null 2>&1 &
