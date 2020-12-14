#! /usr/bin/env bash

# Copyright (c) 2020 Private Internet Access, Inc.
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

function show_usage() {
    echo "usage:"
    echo "$0 [cross_arch] [-- command [args ...]]"
    echo "$0 --help"
    echo ""
    echo "Enters a Debian 9 (Stretch) chroot previously created by setup.sh."
    echo "(See README.md.)  Invoke as normal user to be a normal user in the"
    echo "chroot; invoke as root to be root in the chroot."
    echo ""
    echo "Parameters:"
    echo "  cross_arch: Enter a chroot with cross build dependencies for the"
    echo "    given architecture (created with setup.sh --cross-target arch)."
    echo "    Otherwise, enters a chroot with dependencies for the host"
    echo "    architecture."
    echo "  -- command [args ...]: Run the given command and args in the chroot"
    echo "    instead of entering a shell.  Delimited with '--', args are"
    echo "    forwarded to schroot."
    echo "  --help: Shows this help."
    echo ""
}

ARGS_DONE=""
CROSS_TARGET=""
while [ -z "$ARGS_DONE" ] && [ "$#" -gt 0 ]; do
    case "$1" in
        "--help")
            show_usage
            exit 0
            ;;
        --)
            ARGS_DONE=1
            shift
            ;;
        *)
            CROSS_TARGET="$1"
            shift
            ;;
    esac
done

schroot -c "piabuild-stretch${CROSS_TARGET:+-$CROSS_TARGET}" "$@"
