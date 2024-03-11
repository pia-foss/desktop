#!/bin/bash

# Copyright (c) 2024 Private Internet Access, Inc.
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

# Overwrite PATH with known safe defaults
PATH="/usr/bin:/usr/sbin:/bin:/sbin"

# This script is used by the PIA client to install WireGuard dependencies from
# the GUI.  For distributions that aren't supported for automatic installation,
# the UI shows a message referring to the WireGuard web site.  Otherwise, the
# install is started in a detached terminal emulator.
#
# To implement both of those behaviors, and because we can't pass parameters or
# environment variables through a terminal emulator invocation (see
# run-in-terminal.sh), this script can be run in either "detect" or "install"
# mode.  The UI runs it with --detect just to test whether automatic install is
# available, then if it is, runs it in a detached terminal emulator to perform
# the installation.

function show_usage() {
    echo "usage:"
    echo "$0"
    echo "$0 --detect"
    echo "$0 --help"
    echo ""
    echo "Installs WireGuard kernel module packages for supported distributions."
    echo ""
    echo "With --detect, just detects whether the distribution is supported for"
    echo "automatic installation (exits with result 1 if it is not)."
    echo ""
}

detect_only=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        "--help")
            show_usage
            exit 0
            ;;
        "--detect")
            detect_only=1
            ;;
        *)
            echo "Unknown option $1" >2
            show_usage
            exit 1
            ;;
    esac
    shift
done

function requestConfirmation() {
    read -r -p "$1 Continue? [y/N] " input

    case $input in
    [yY][eE][sS]|[yY])
        ;;

    [nN][oO]|[nN])
        exit 0
            ;;

    *)
    exit 0
    ;;
    esac
}

function getUbuntuVersion() {
    UBUNTU_MAJOR=0
    UBUNTU_MINOR=0
    # Must have lsb_release
    if ! hash lsb_release 2>/dev/null; then
        return 0
    fi
    # Distributor ID must be "Ubuntu"
    if ! [[ "$(lsb_release -i)" =~ :[[:blank:]]+Ubuntu$ ]]; then
        return 0
    fi
    # Split the version number
    if [[ "$(lsb_release -r)" =~ :[[:blank:]]+([0-9]+)\.([0-9]+)$ ]]; then
        UBUNTU_MAJOR="${BASH_REMATCH[1]}"
        UBUNTU_MINOR="${BASH_REMATCH[2]}"
    fi
}

# The package manager detection and precedence are similar to the logic in the
# install script.
#
# We don't need wireguard-tools (though it doesn't hurt anything), but we still
# install it - users might be confused to find the wireguard kernel module but
# no tools, and if there is a problem, it's likely that we could ask for
# diagnostic info from 'wg'.
if hash yum 2>/dev/null; then
    # yum applies to REHL and CentOS, the steps to install WireGuard vary by
    # release and in some cases involve installing RPMs from Fedora.  Direct the
    # user to the WireGuard instructions
    echo "Automatic install not available on yum distributions" >2
    exit 1
elif hash pacman 2>/dev/null; then
    # Arch is not supported for automatic installation, no good single solution
    # without knowing more about the user's system.  User needs to install
    # wireguard-dkms or wireguard-lts for the kernel module:
    #
    # * wireguard-dkms does not install kernel headers, user needs appropriate
    #   header package too
    # * wireguard-lts also depends on linux-lts, we don't want to install a
    #   kernel if the user is using a different kernel
    echo "Automatic install not available on pacman distributions" >2
    exit 1
elif hash zypper 2>/dev/null; then
    if [ "$detect_only" ]; then
        echo "Using openSUSE installation for zypper distribution"
        exit 0
    fi
    requestConfirmation "This will add the WireGuard repository, then install wireguard-kmp-default and wireguard-tools."
    sudo zypper addrepo -f obs://network:vpn:wireguard wireguard
    sudo zypper install wireguard-kmp-default wireguard-tools
elif hash apt-get 2>/dev/null; then
    # Only Ubuntu is supported for apt distributions currently.
    # Installing on Debian requires pulling packages from unstable, which
    # Debian docs specifically warn against - the user should be aware of this
    # change to accept the risk.
    # The WireGuard instructions try to set priority 90 for the unstable repo to
    # avoid pulling in other updates unintentionally, but this might conflict
    # with other changes the user has made for some configurations.
    getUbuntuVersion
    if [[ "$UBUNTU_MAJOR" -ge 20 ]] || [[ "$UBUNTU_MAJOR.$UBUNTU_MINOR" == "19.10" ]]; then
        if [ "$detect_only" ]; then
            echo "Using Ubuntu distribution package installation for version $UBUNTU_MAJOR.$UBUNTU_MINOR"
            exit 0
        fi
        requestConfirmation "This will install the wireguard package."
        sudo apt-get install -y wireguard
    elif [[ "$UBUNTU_MAJOR" -gt 0 ]]; then
        if [ "$detect_only" ]; then
            echo "Using Ubuntu PPA package installation for version $UBUNTU_MAJOR.$UBUNTU_MINOR"
            exit 0
        fi
        requestConfirmation "This will add the WireGuard PPA, then install the wireguard package."
        sudo add-apt-repository -y ppa:wireguard/wireguard
        sudo apt-get update
        sudo apt-get install -y wireguard
    else
        echo "Automatic install not available for non-Ubuntu apt distribution" >2
        # Other distribution, refer to manual instructions
        exit 1
    fi
else
    echo "Could not detect a supported package manager" >2
    exit 1
fi
