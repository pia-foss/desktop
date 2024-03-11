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

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

logFile="/dev/null"

readonly appName="{{BRAND_NAME}}"
readonly brandCode="{{BRAND_CODE}}"
readonly installDir="/opt/${brandCode}vpn"
readonly serviceName="${brandCode}vpn"
readonly groupName="${brandCode}vpn"
readonly hnsdGroupName="${brandCode}hnsd"         # The group used by the Handshake DNS service
readonly routingTableName="${serviceName}rt"
readonly vpnOnlyroutingTableName="${serviceName}Onlyrt"
readonly wireguardRoutingTableName="${serviceName}Wgrt"
readonly forwardedRoutingTableName="${serviceName}Fwdrt" # For forwarded packets
readonly ctlExecutableName="{{BRAND_CODE}}ctl"
readonly ctlExecutablePath="${installDir}/bin/${ctlExecutableName}"
readonly ctlSymlinkPath="/usr/local/bin/${ctlExecutableName}"
readonly wgIfPrefix="wg${brandCode}"                                   # WireGuard interface prefix, e.g wgpia
readonly nmConfigDir="/etc/NetworkManager/conf.d"
readonly nmConfigPath="${nmConfigDir}/${wgIfPrefix}.conf"  # Our custom NetworkManager config

function enableLogging() {
    logFile="/tmp/pia_install.log"
    [ -f $logFile ] && rm $logFile
}
function echoPass() {
    printf '\e[92m\xE2\x9C\x94\e[0m %s\n' "$@"
    echo "$(date +"[%Y-%m-%d %H:%M:%S]") [ OK ] $@" >> "$logFile" || true
}
function echoFail() {
    printf '\e[91m\xE2\x9C\x98\e[0m %s\n' "$@"
    echo "$(date +"[%Y-%m-%d %H:%M:%S]") [FAIL] $@" >> "$logFile" || true
}
function fail() {

    echoFail "$@"
    exit 1
}

confirm() {
    read -r -p "${1:-Proceed with uninstall? [y/N]} " response
    case "$response" in
        [yY][eE][sS]|[yY])
            true
            ;;
        *)
            exit 1
            ;;
    esac
}

function disableDaemon() {
    local systemdServiceLocation="/etc/systemd/system/${serviceName}.service"
    local sysvinitServiceLocation="/etc/init.d/${serviceName}"
    local openrcServiceLocation="/etc/init.d/${serviceName}"

    if [ -f $systemdServiceLocation ]; then
        sudo systemctl stop $serviceName
        echoPass "Stopped daemon"
        sudo systemctl disable $serviceName
        echoPass "Disabled daemon"
        sleep 1
        sudo rm $systemdServiceLocation
    # openrcServiceLocation is set to the same file as sysvinitServiceLocation so we need the extra rc-status check
    # to distinguish it from a sysvinit system
    elif rc-status > /dev/null 2>&1 && [ -f "$openrcServiceLocation" ]; then
        sudo rc-service $serviceName stop
        echoPass "Stopped daemon"
        sudo rc-update del $serviceName
        echoPass "Disabled daemon"
        sleep 1
        sudo rm $openrcServiceLocation
    elif [ -f $sysvinitServiceLocation ]; then
        sudo service $serviceName stop
        echoPass "Stopped daemon"
        sudo update-rc.d $serviceName remove
        echoPass "Disabled daemon"
        sleep 1
        sudo rm $sysvinitServiceLocation
     fi
}

function removeGroups() {
    for group in "$@"; do
        if grep -q $group /etc/group; then
            sudo groupdel $group || true
        fi
    done
    true
}

function removeRoutingTable() {
    local routesLocation=/etc/iproute2/rt_tables
    local routingTable="$1"

    if [[ -f $routesLocation ]] && grep -q "$routingTable" $routesLocation; then
        grep -v "$routingTable" $routesLocation | sudo tee $routesLocation > /dev/null
        echoPass "Removed $routingTable routing table"
    fi

    true
}

function removeWireguardUnmanaged() {
    [ -f "$nmConfigPath" ] && sudo rm -f "$nmConfigPath"
}

if [[ "$1" == "startuninstall" ]] ; then
    echo "==================================="
    echo "$appName Uninstaller"
    echo "==================================="
    echo ""
    confirm
    killall "${brandCode}-client"
    echoPass "Stopped existing client"
    sleep 1
    disableDaemon
    sudo rm "/usr/share/applications/${brandCode}vpn.desktop"

    # Remove the piactl symlink, as long as it points to the correct piactl
    # (this is very paranoid since it's in /usr/local/bin)
    existingSymlinkTarget=$(readlink "${ctlSymlinkPath}" || true)
    if [ "${existingSymlinkTarget}" = "${ctlExecutablePath}" ]; then
        sudo rm "${ctlSymlinkPath}"
    fi

    [ -f "~/.config/autostart/${brandCode}vpn.desktop" ] && rm "~/.config/autostart/${brandCode}vpn.desktop"
    [ -f "/usr/share/pixmaps/${brandCode}vpn.png" ] && sudo rm "/usr/share/pixmaps/${brandCode}vpn.png"
    [ -f "/etc/apport/blacklist.d/${brandCode}vpn" ] && sudo rm "/etc/apport/blacklist.d/${brandCode}vpn"
    [ -e ~/.config/privateinternetaccess ] && rm -rf ~/.config/privateinternetaccess/
    echoPass "Removed $appName System files"
    sudo rm -rf "/opt/${brandCode}vpn/"
    removeGroups $groupName $hnsdGroupName
    removeRoutingTable $routingTableName
    removeRoutingTable $vpnOnlyroutingTableName
    removeRoutingTable $wireguardRoutingTableName
    removeRoutingTable $forwardedRoutingTableName
    removeWireguardUnmanaged
    echoPass "Uninstall finished"
    read -n 1 -s -r -p "Press any key to continue"
else
    # Try to generate a temporary file for writing the uninstaller. Otherwise
    # Fall back onto a hard-coded path.
    UNINSTALL_SCRIPT="/tmp/$brandCode-uninstall"

    if [ hash tempfile 2>/dev/null ]; then
        UNINSTALL_SCRIPT=`tempfile`
    fi

    [ -f $UNINSTALL_SCRIPT ] && rm $UNINSTALL_SCRIPT
    cp -f $0 $UNINSTALL_SCRIPT
    chmod +x $UNINSTALL_SCRIPT

    $UNINSTALL_SCRIPT startuninstall
fi
