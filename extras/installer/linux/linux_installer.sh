#!/bin/bash

# Copyright (c) 2019 London Trust Media Incorporated
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

set -e


# Overwrite PATH with known safe defaults
PATH="/usr/bin:/usr/sbin:/bin:/sbin"

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

logFile="/dev/null"

readonly appName="{{BRAND_NAME}}"
readonly brandCode="{{BRAND_CODE}}"
readonly installDir="/opt/${brandCode}vpn"
readonly daemonSettingsPath="$installDir/etc"
readonly oldSettingsPath="/$HOME/.pia_manager/data"
readonly systemdServiceLocation="/etc/systemd/system/${brandCode}vpn.service"
readonly sysvinitServiceLocation="/etc/init.d/${brandCode}vpn"
readonly openrcServiceLocation="/etc/init.d/${brandCode}vpn"
readonly serviceName="${brandCode}vpn"
readonly groupName="${brandCode}vpn"
readonly hnsdGroupName="${brandCode}hnsd"         # The group used by the Handshake DNS service
readonly routingTableName="${serviceName}rt"
readonly ctlExecutableName="{{BRAND_CODE}}ctl"
readonly ctlExecutablePath="${installDir}/bin/${ctlExecutableName}"
readonly ctlSymlinkPath="/usr/local/bin/${ctlExecutableName}"

echo ""
echo "================================="
echo "$appName Installer"
echo "================================="
echo ""

function enableLogging() {
    logFile="/tmp/pia_install.log"
}

function echoPass() {
    printf '\e[92m\xE2\x9C\x94\e[0m %s\n' "$@"
    echo "$(date +"[%Y-%m-%d %H:%M:%S]") [ OK ]" "$@" >> "$logFile" || true
}

function echoFail() {
    printf '\e[91m\xE2\x9C\x98\e[0m %s\n' "$@"
    echo "$(date +"[%Y-%m-%d %H:%M:%S]") [FAIL]" "$@" >> "$logFile" || true
}

function fail() {
    echoFail "$@"
    exit 1
}

function configureSystemd() {
    # install the service
    sudo cp "$root/installfiles/piavpn.service" "$systemdServiceLocation"

    echoPass "Created $serviceName service"

    sudo systemctl daemon-reload
    sudo systemctl enable "$serviceName"
    sudo systemctl restart "$serviceName"
    echoPass "Started $serviceName service"

    # Start the client
    "$installDir/bin/${brandCode}-client" > /dev/null 2>&1 & disown
    true
}

function configureSysvinit() {
    sudo cp "$root/installfiles/piavpn.sysvinit.service" "$sysvinitServiceLocation"
    sudo chmod 755 "$sysvinitServiceLocation"
    sudo update-rc.d "$serviceName" defaults
    sudo service "$serviceName" start

    echoPass "Created $serviceName sysvinit service"

    # Start the client
    "$installDir/bin/${brandCode}-client" > /dev/null 2>&1 & disown
    true
}

function configureOpenrc() {
    sudo cp "$root/installfiles/piavpn.openrc.service" "$openrcServiceLocation"
    sudo chmod 755 "$openrcServiceLocation"
    sudo rc-update add $serviceName default
    sudo rc-service $serviceName start

    echoPass "Created $serviceName openrc service"

    # Start the client
    "$installDir/bin/${brandCode}-client" > /dev/null 2>&1 & disown
    true
}

function exemptFromApport() {
    # exempt piavpn from apport blacklist
    if [ -d /etc/apport/blacklist.d ]; then
        sudo tee "/etc/apport/blacklist.d/${brandCode}vpn" > /dev/null << EOF
        $installDir/bin/${brandCode}-client
        $installDir/bin/${brandCode}-daemon
EOF
        systemctl is-active --quiet apport && sudo service apport restart
    fi
    true
}

function removeLegacyPia() {
    if [[ -d "/opt/pia/" ]]; then
        # Show a disclaimer about replacing older version of PIA
        echo "This will replace your existing installation of $appName."
        requestConfirmation "Downgrading afterwards will require a clean reinstall. Do you wish to continue?"
        while pgrep -f "/opt/pia/" > /dev/null; do
            read -rsn1 -p"Please exit Private Internet Access and press any key to continue."
            echo ""
        done
        sudo rm -rf /opt/pia
        [ -f "$HOME/pia.sh" ] && sudo rm "$HOME/pia.sh"
        [ -f "$HOME/.local/share/applications/pia_manager.desktop" ] && sudo rm "$HOME/.local/share/applications/pia_manager.desktop"
        echoPass "Uninstalled $appName"
    fi
    true
}

# Test whether dependencies are present.  Returns nonzero if any dependency is
# missing.
function hasDependencies() {
    # Check for ifconfig, libxkbcommon-x11, libxkbcommon, and libxcb-xkb
    # Note that ifconfig must be at /sbin/ifconfig since OpenVPN finds the
    # absolute path to ifconfig at build time.

    # Wrap each test in `if ...; then return 1; fi` to play nicely with set -e
    if ! [ -x /sbin/ifconfig ]; then return 1; fi
    if ! ldconfig -p | grep -q libxkbcommon-x11.so.0; then return 1; fi
    return 0
}

function promptNetTools() {
    echo "Could not install package 'net-tools'."
    echo "Please install the package manually and ensure 'ifconfig' command exists."
}

function promptManualDependencies() {
    echo "Could not install dependencies.  Please install these packages:"
    echo " - net-tools (ifconfig)"
    echo " - libxkbcommon-x11 (libxkbcommon-x11.so.0, libxkbcommon.so.0, libxcb-xkb.so.1)"
    requestConfirmation "Continue with installation?"
}

function installDependencies() {
    # If all dependencies are present, don't do anything, don't try to detect
    # package manager, etc.
    if hasDependencies; then return 0; fi

    if hash yum 2>/dev/null; then
        sudo yum -y install net-tools libxkbcommon-x11
    elif hash pacman 2>/dev/null; then
        sudo pacman -S --noconfirm net-tools libxkbcommon-x11
    elif hash zypper 2>/dev/null; then
        sudo zypper install libxkbcommon-x11-0
        # We can't set up ifconfig on openSUSE; our OpenVPN build has a
        # hard-coded path to /sbin/ifconfig, but openSUSE installs it to
        # /usr/bin/ifconfig.  We don't want to mess with the user's sbin
        # directory, the user will have to make the symlink themselves.
        echoFail "ifconfig not installed - please ensure net-tools-deprecated is installed and symlink ifconfig to /sbin/ifconfig"
    # Check for apt-get last.  Apparently some RPM-based distributions (such as
    # openSUSE) have an RPM port of apt in addition to their preferred package
    # manager.  This check uses Debian package names though that aren't
    # necessarily the same on other distributions.
    elif hash apt-get 2>/dev/null; then
        # A few releases do not have the net-tools package at all, still try to
        # install other dependencies
        APT_PKGS="libxkbcommon-x11-0"
        if [[ $(apt-cache search --names-only net-tools) ]]; then
            sudo apt-get install --yes net-tools $APT_PKGS
        else
            sudo apt-get install --yes $APT_PKGS
            promptNetTools
        fi
    else
        promptManualDependencies
        return 0 # Skip "installed packages" output
    fi
    echoPass "Installed packages"
}

function addGroups() {
    for group in "$@"; do
        if ! grep -q $group /etc/group; then
            sudo groupadd $group || true
            echoPass "Added group $group"
        fi
    done

    true
}

function installPia() {
    addGroups $groupName $hnsdGroupName

    sudo mkdir -p $installDir

    if [[ $(pgrep "${brandCode}-client") ]]; then
        killall "${brandCode}-client"
        sleep 2
        echoPass "Closed running $appName client"
    fi

    # Clear old PIA files if existing
    # We are deleting specific folders rather than the alternative (remove all but exclude data dir)
    sudo rm -rf "$installDir/bin"
    sudo rm -rf "$installDir/lib"
    sudo rm -rf "$installDir/plugins"
    sudo rm -rf "$installDir/qml"

    # Use /bin/cp and not 'cp' since 'cp' might be aliased
    sudo /bin/cp -rf "$root/piafiles/"* $installDir/
    sudo cp "$root/installfiles/run-in-terminal.sh" $installDir/bin/
    sudo cp "$root/installfiles/pia-uninstall.sh" "$installDir/bin/$brandCode-uninstall.sh"
    sudo chmod +x "$installDir/bin/$brandCode-uninstall.sh" "$installDir/bin/run-in-terminal.sh"
    echoPass "Copied $appName files"

    # Allow us to run handshake without root
    if sudo setcap 'cap_net_bind_service=+ep' "$installDir/bin/pia-hnsd" 2>/dev/null; then
        echoPass "Allow non-root $installDir/bin/pia-hnsd to bind to privileged ports"
    fi

    sudo mkdir -p $installDir/var
    echoPass "Created var folder"

    # Ideally we don't want to put them in pixmaps but this seems to be the
    # best place for cross platform usage, and is part of the XDG spec
    # Some platforms like Arch do't have this path
    if [ ! -d "/usr/share/pixmaps" ]; then
        sudo mkdir -p /usr/share/pixmaps/
    fi
    sudo cp "$root/installfiles/app.png" "/usr/share/pixmaps/${brandCode}.png"
    echoPass "Installed icon"

    if [ ! -d "/usr/share/applications" ]; then
        sudo mkdir -p /usr/share/applications/
    fi
    sudo cp "$root/installfiles/piavpn.desktop" "/usr/share/applications/${brandCode}vpn.desktop"
    echoPass "Created desktop entry"

    # Create PIA routing table for split-tunneling
    local highestIndex=$(awk '/^[0-9]/{print $1}' /etc/iproute2/rt_tables | sort -n | tail -1)
    local newIndex=$(($highestIndex + 1))

    local routesLocation=/etc/iproute2/rt_tables
    if [[ -f $routesLocation ]] && ! grep -q "$routingTableName" $routesLocation; then
        echo -e "$newIndex\t$routingTableName" | sudo tee -a $routesLocation > /dev/null
        echoPass "Added $routingTableName routing table"
    fi

    # Link piactl into /usr/local/bin if it's not already there.  If it's there,
    # either it's ours and doesn't need to be updated, or it's something else
    # and we shouldn't touch it.?
    if [ ! -L "${ctlSymlinkPath}" ] && [ ! -e "${ctlSymlinkPath}" ]; then
        # This is allowed to fail if /usr/bin doesn't exist for some reason,
        # etc. - just ignore it
        sudo ln -s "${ctlExecutablePath}" "${ctlSymlinkPath}" || true
    fi

    true
}

function migrateLegacySettings() {
    if [ -f "$oldSettingsPath/settings.json" ] && [ ! -f "$daemonSettingsPath/settings.json" ]; then
        if echo "{\"legacy\":$(cat "$oldSettingsPath/settings.json")}" | sudo tee "$daemonSettingsPath/settings.json" > /dev/null; then
            echoPass "Migrated old settings"
        else
            echoFail "Old settings not migrated"
        fi
    fi

    # Clean up files after legacy settings migrated
    [ -e "$HOME/.pia_manager" ] && sudo rm -rf "$HOME/.pia_manager"
    [ -e "$HOME/.com.privateinternetaccess.vpn" ] && sudo rm -rf "$HOME/.com.privateinternetaccess.vpn"
    true
}

function autoDetectSystem() {
    # ignore "ps | grep" warning (below in elif) pgrep cannot check a specific PID
    # shellcheck disable=SC2009
    # if prior systemd install exists, assume systemd
    if [ -f "$systemdServiceLocation" ]; then
        echo "Detected a previous systemd install - assuming systemd"
        BOOT_MANAGER=systemd
    # openrcServiceLocation is set to the same file as sysvinitServiceLocation so we need the extra rc-status check
    # to distinguish it from a sysvinit system
    elif rc-status > /dev/null 2>&1 && [ -f "$openrcServiceLocation" ]; then
        echo "Detected a previous openrc install - assuming openrc"
        BOOT_MANAGER=openrc
    # if prior sysvinit install exists, assume sysvinit
    elif [ -f "$sysvinitServiceLocation" ]; then
        echo "Detected a previous sysvinit install - assuming sysvinit"
        BOOT_MANAGER=sysvinit
    # pia is installed but no service detected, assume "none"
    elif [ -f "$installDir/bin/${brandCode}-daemon" ]; then
        echo "Detected a previous install but no service installed - assuming no service should be configured"
        BOOT_MANAGER=none
    # rc-status command should only exist on openrc systems
    elif rc-status > /dev/null 2>&1; then
        BOOT_MANAGER=openrc
    # make best guess at the system type
    elif ps -p 1 | grep -q init; then
        BOOT_MANAGER=sysvinit
    else
        # fall-back to systemd if it's not sysvinit
        BOOT_MANAGER=systemd
    fi
    true
}

function processOpts() {
    case "$1" in
    --sysvinit)
        BOOT_MANAGER=sysvinit
        ;;
    --systemd)
        BOOT_MANAGER=systemd
        ;;
    --openrc)
        BOOT_MANAGER=openrc
        ;;
    --skip-service)
        BOOT_MANAGER=none
        ;;
    -h|--help)
        echo "Usage: $brandCode-linux-<version>.run -- <install-options>"
        echo "Install options:"
        echo "  --systemd to setup a systemd service on boot"
        echo "  --sysvinit to setup a sysvinit service on boot"
        echo "  --skip-service to skip setting up a service"
        exit 0
        ;;
    *)
        echo "Unrecognized option: '$1'"
        echo "Type: 'pia-linux-<version>.run -- -h' to see the available options."
        exit 1
        ;;
    esac
    true
}

function requestConfirmation() {
    read -r -p "$1 [y/N] " input

    case $input in
    [yY][eE][sS]|[yY])
        ;;

    [nN][oO]|[nN])
        exit 1
            ;;

    *)
    exit 1
    ;;
    esac
}

if [[ $EUID -eq 0 ]]; then
    echo "This script must be run as a normal user, not root."
    exit 1
fi

# process command line options and figure out the system type (e.g systemd vs sysvinit)
if [[ $# -eq 0 ]]; then
    autoDetectSystem
else
    processOpts "$1"
fi

# early-exit in our default case (systemd) if systemd is not installed
if [[ $BOOT_MANAGER == systemd ]] && ! hash systemctl 2>/dev/null; then
    echo "systemd installation was selected, but systemctl was not found."
    echo "Use ${brandCode}-linux-<version>.run -- --help for options to select a different installation."

    exit 1
fi

ORIG_UMASK=$(umask)

# Ensure that all files are world readable
umask 022

if [ $brandCode = "pia" ]; then
    removeLegacyPia
fi
installDependencies
installPia
if [ $brandCode = "pia" ]; then
    migrateLegacySettings
fi

case "$BOOT_MANAGER" in
systemd)
    exemptFromApport
    configureSystemd
    ;;
sysvinit)
    configureSysvinit
    ;;
openrc)
    configureOpenrc
    ;;
none)
    echoPass "Finished. You will need to manually configure ${installDir}/bin/${brandCode}-daemon to start at boot."
    ;;
*)
    echoFail "Error: BOOT_MANAGER had unexpected value, got $BOOT_MANAGER"
    ;;
esac

# Restore the original umask
umask "$ORIG_UMASK"
