#!/bin/bash

# Copyright (c) 2023 Private Internet Access, Inc.
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
readonly buildArchitecture="{{BUILD_ARCHITECTURE}}"
readonly installDir="/opt/${brandCode}vpn"
readonly daemonSettingsPath="$installDir/etc"
readonly daemonDataPath="$installDir/var"
readonly daemonResPath="$installDir/share"
readonly oldSettingsPath="/$HOME/.pia_manager/data"
readonly systemdServiceLocation="/etc/systemd/system/${brandCode}vpn.service"
readonly sysvinitServiceLocation="/etc/init.d/${brandCode}vpn"
readonly openrcServiceLocation="/etc/init.d/${brandCode}vpn"
readonly serviceName="${brandCode}vpn"
readonly groupName="${brandCode}vpn"
readonly hnsdGroupName="${brandCode}hnsd"                # The group used by the Handshake DNS service
readonly ctlExecutableName="{{BRAND_CODE}}ctl"
readonly ctlExecutablePath="${installDir}/bin/${ctlExecutableName}"
readonly ctlSymlinkPath="/usr/local/bin/${ctlExecutableName}"
readonly wgIfPrefix="wg${brandCode}"                       # WireGuard interface prefix, e.g wgpia
readonly nmConfigDir="/etc/NetworkManager/conf.d"
readonly nmConfigPath="${nmConfigDir}/${wgIfPrefix}.conf"  # Our custom NetworkManager config

# Set by arguments, or auto-detected
BOOT_MANAGER=
FORCE_ARCHITECTURE=

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

# This ensures that NetworkManager does not remove DNS settings from the wireguard interface when
# the user changes network.
function wireguardUnmanaged() {
    if [ -d "$nmConfigDir" ] && [ ! -f "$nmConfigPath" ]; then
       echo -e "[keyfile]\nunmanaged-devices=interface-name:${wgIfPrefix}*" | sudo tee "$nmConfigPath" > /dev/null
       echoPass "Set $wgIfPrefix interface to be unmanaged"
    fi
}

function startClient() {
    # Change directory before starting the client; don't start it from a
    # directory that's about to be deleted.
    # The PIA client doesn't really care, but some of the programs it starts
    # might (in particular, Terminator does, and it might be the user's
    # preferred terminal emulator for updates / installation)
    pushd "$installDir/bin/" >/dev/null 2>&1
    nohup "$installDir/bin/${brandCode}-client" --clear-cache >/dev/null 2>&1 &
    popd >/dev/null 2>&1
    true
}

function configureSystemd() {
    # install the service
    sudo cp "$root/installfiles/piavpn.service" "$systemdServiceLocation"
    sudo chmod 644 "$systemdServiceLocation"

    echoPass "Created $serviceName service"

    sudo systemctl daemon-reload
    sudo systemctl enable "$serviceName"
    sudo systemctl restart "$serviceName"
    echoPass "Started $serviceName service"

    startClient
}

function configureSysvinit() {
    sudo cp "$root/installfiles/piavpn.sysvinit.service" "$sysvinitServiceLocation"
    sudo chmod 755 "$sysvinitServiceLocation"
    sudo update-rc.d "$serviceName" defaults
    # If this is an upgrade, the daemon exited when we deleted the file (it
    # detects that), but the init script still has a PID file and won't start
    # it again until we stop it first.  Ignore failures from the stop, also.
    sudo service "$serviceName" stop 2>/dev/null || true
    sudo service "$serviceName" start

    echoPass "Created $serviceName sysvinit service"

    startClient
}

function configureOpenrc() {
    sudo cp "$root/installfiles/piavpn.openrc.service" "$openrcServiceLocation"
    sudo chmod 755 "$openrcServiceLocation"
    sudo rc-update add $serviceName default
    sudo rc-service $serviceName start

    echoPass "Created $serviceName openrc service"

    startClient
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
    # Check for libxkbcommon-x11, libxkbcommon, and libxcb-xkb

    # Wrap each test in `if ...; then return 1; fi` to play nicely with set -e
    if ! ldconfig -p | grep -q libxkbcommon.so.0; then return 1; fi
    if ! ldconfig -p | grep -q libxkbcommon-x11.so.0; then return 1; fi
    if ! ldconfig -p | grep -q libnl-3.so.200; then return 1; fi
    if ! ldconfig -p | grep -q libnl-route-3.so.200; then return 1; fi
    if ! ldconfig -p | grep -q libnl-genl-3.so.200; then return 1; fi
    if ! ldconfig -p | grep -q libnsl.so.1; then return 1; fi
    # The iptables binary is usually located in the /usr/sbin directory.
    # Another way of checking, more in line with the ones above, could be:
    # if ! ls /usr/sbin | grep -q iptables; then return 1; fi
    # The chosen method could be more resistant if in some distros the
    # binary is in another directory
    if ! iptables --version >/dev/null 2>&1; then return 1; fi

    return 0
}

function promptManualDependencies() {
    echo "Could not install dependencies.  Please install these packages:"
    echo " - libxkbcommon-x11 (libxkbcommon-x11.so.0, libxkbcommon.so.0)"
    echo " - libnl-3-200"
    echo " - libnl-route-3-200, libnl-genl-3-200 (may be included in libnl-3-200)"
    echo " - libnsl (libnsl.so.1)"
    echo " - iptables"
    requestConfirmation "Continue with installation?"
}

function installDependencies() {
    # If all dependencies are present, don't do anything, don't try to detect
    # package manager, etc.
    if hasDependencies; then return 0; fi

    # Here we are installing the "iptables" package in every distribution
    # This, depending on the distribution, can be the legacy one or the
    # nftables backend one.
    #
    # Debian:   "iptables" uses nf_tables backend
    # Fedora:   "iptables" is the legacy package installed by default
    #           "iptables-nft" is nf_tables backend one
    # Manjaro:  "iptables" is the legacy package installed by default
    #           "iptables-nft" is nf_tables backend one
    #           they can't both be installed at the same time, installing one will remove the other
    # OpenSUSE: "iptables" is the legacy package installed by default
    #           "iptables-backend-nft" is nf_tables backend one
    #
    # All these packages are valid, if the user already installed a different one
    # then the one we install here, there should be no problems.
    #
    # All the distributions also have a "nftables" package
    # This is another package that does not depend on iptables in any way, or viceversa.
    # Both can be installed at the same on a system without any conflicts.

    if hash yum 2>/dev/null; then
        # libnsl is no longer part of of the glibc package on Fedora
        sudo yum -y install libxkbcommon-x11 libnl3 libnsl iptables psmisc
    elif hash pacman 2>/dev/null; then
        sudo pacman -S --noconfirm libxkbcommon-x11 libnl iptables psmisc
    elif hash zypper 2>/dev/null; then
        sudo zypper install -y libxkbcommon-x11-0 iptables psmisc
    # Check for apt-get last.  Apparently some RPM-based distributions (such as
    # openSUSE) have an RPM port of apt in addition to their preferred package
    # manager.  This check uses Debian package names though that aren't
    # necessarily the same on other distributions.
    elif hash apt-get 2>/dev/null; then
        APT_PKGS="libxkbcommon-x11-0 libnl-3-200 libnl-route-3-200 iptables psmisc"
        sudo apt-get install --yes $APT_PKGS
    else
        promptManualDependencies
        return 0 # Skip "installed packages" output
    fi
    echoPass "Installed packages"
}

function addGroups() {
    for group in "$@"; do
        if ! getent group $group >/dev/null 2>&1; then
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
    sudo /bin/cp "$root/installfiles/"*.sh "$installDir/bin/"
    sudo chmod +x "$installDir/bin/"*.sh
    echoPass "Copied $appName files"

    # Allow us to run unbound without root
    if sudo setcap 'cap_net_bind_service=+ep' "$installDir/bin/pia-unbound" 2>/dev/null; then
        echoPass "Allow non-root $installDir/bin/pia-unbound to bind to privileged ports"
    fi

    sudo mkdir -p "$daemonDataPath"
    echoPass "Created var folder"

    # Ideally we don't want to put them in pixmaps but this seems to be the
    # best place for cross platform usage, and is part of the XDG spec
    # Some platforms like Arch do't have this path
    if [ ! -d "/usr/share/pixmaps" ]; then
        sudo mkdir -p /usr/share/pixmaps/
    fi
    sudo cp "$root/installfiles/app-icon.png" "/usr/share/pixmaps/${brandCode}vpn.png"
    [ -e "/usr/share/pixmaps/${brandCode}.png" ] && sudo rm -rf "/usr/share/pixmaps/${brandCode}.png"
    echoPass "Installed icon"

    if [ ! -d "/usr/share/applications" ]; then
        sudo mkdir -p /usr/share/applications/
    fi
    sudo cp "$root/installfiles/piavpn.desktop" "/usr/share/applications/${brandCode}vpn.desktop"
    if hash update-desktop-database 2>/dev/null; then
        # Silence output from update-desktop-database; it dumps errors for _any_
        # quirks in installed .desktop files, not just ours.
        sudo update-desktop-database 2>/dev/null
    fi
    echoPass "Created desktop entry"

    # Instruct NetworkManager not to interfere with DNS on the wireguard interface
    wireguardUnmanaged

    # Link piactl into /usr/local/bin if it's not already there.  If it's there,
    # either it's ours and doesn't need to be updated, or it's something else
    # and we shouldn't touch it.?
    if [ ! -L "${ctlSymlinkPath}" ] && [ ! -e "${ctlSymlinkPath}" ]; then
        # This is allowed to fail if /usr/bin doesn't exist for some reason,
        # etc. - just ignore it
        sudo ln -s "${ctlExecutablePath}" "${ctlSymlinkPath}" || true
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

# This checks uname -m to try to figure out if there is a native PIA build
# available for this platform, and what the PIA build architecture is if so.
#
# Prints the PIA architecture name (x86_64, armhf, or arm64 - matches build.rb),
# or nothing if no such build is known.
#
# Note that this is _not_ 100% reliable as discussed in
# checkArchitectureSupport and is just used for advisory purposes to suggest a
# native build when available.
function guessNativeArchitecture() {
    case "$(uname -m)" in
        amd64|x86_64)
            echo "x86_64"
            ;;
        aarch64*|armv8*)
            echo "arm64"
            ;;
        # This is particularly vague because we don't actually know what ABI
        # the system is using from this information.  Most ARM systems have
        # moved to 64-bit though, and the remaining 32-bit systems are mostly
        # armhf, so guess that this is likely armhf.
        arm*)
            echo "armhf"
            ;;
        *)
            # Something else
            ;;
    esac
}

function checkBestArchitecture() {
    # This build can be installed, but if we think a different build is native
    # for this system, suggest that build instead.
    if [ -n "$HOST_PIA_ARCHITECTURE" ] && [ "$HOST_PIA_ARCHITECTURE" != "$buildArchitecture" ]; then
        echo "This is the $buildArchitecture build, which may be running under emulation."
        echo "Consider using the $HOST_PIA_ARCHITECTURE build instead, which may be native for your system."
        requestConfirmation "Continue installing $buildArchitecture?"
    fi
}

function checkAlternateArchitecture() {
    # This build can't be installed.  If we think a different build is native
    # for this system, suggest that build instead.
    if [ -n "$HOST_PIA_ARCHITECTURE" ] && [ "$HOST_PIA_ARCHITECTURE" != "$buildArchitecture" ]; then
        echo "Install the $HOST_PIA_ARCHITECTURE build instead, this system does not support $buildArchitecture."
        exit 1
    else
        # Either we do not know the native architecture for this system, or
        # we think this build actually is the native architecture (we may be
        # wrong, or there may be another reason the build does not work, such as
        # a glibc version mismatch).
        echo "This build does not appear to be compatible with this system."
        echo "This may be due to an architecture or library mismatch."
        echo "If your distribution is recent and supported, consider trying a different build of PIA."
        requestConfirmation "Continue installing $buildArchitecture?"
    fi
}

function promptIncompatibleUpgrade() {
    echo "This upgrade does not appear to be compatible with this system."
    echo "If this is an older distribution, it may no longer be supported."
    echo ""
    echo "The PIA installation will not be modified."
    echo "You can stop receiving update notifications if this distribution is out of support."
    requestConfirmation "Stop receiving update notifications?"
    # If the user answers 'no', requestConfirmation exits.
    # Disable update notifications by clearing the update channels the daemon
    # stops checking for updates when this is done.
    "/opt/${brandCode}vpn/bin/${brandCode}ctl" --unstable applysettings '{"updateChannel":"","betaUpdateChannel":""}'
}

function isBinExecutable() {
    local BIN="$1"
    # Wrap each test in `if ...; then return 1; fi` to play nicely with set -e
    if ! "$BIN" --version >/dev/null 2>&1; then
        # Not compatible, show the specific error (will indicate incompatible
        # architecture or missing library, etc.)
        echoFail "Build is not compatible with this system."
        echo "$BIN --version"
        "$BIN" --version
        echo ""
        return 1
    fi
}

function isBuildExecutable() {
    # The CLI is the most basic test; it has few dependencies.
    if ! isBinExecutable "$root/piafiles/bin/${brandCode}ctl"; then return 1; fi
    # Test the daemon too.  It may refer to specific versioned symbols from
    # libstdc++ that aren't used by the CLI.  Note that we haven't installed the
    # libnl dependencies yet, but that's OK, those are loaded at runtime.
    if ! isBinExecutable "$root/piafiles/bin/${brandCode}-daemon"; then return 1; fi
    # We can't test the client, as the XCB dependencies have not been installed
    # yet.
    return 0
}

function checkArchitectureSupport() {
    HOST_PIA_ARCHITECTURE="$(guessNativeArchitecture)"
    echo "Installing PIA for $buildArchitecture, system is $(uname -m)"

    # If PIA is already installed with this architecture, assume we can upgrade
    # it, even if it doesn't seem to be executable.  This indicates that the
    # user overrode detection at some point in the past, we don't want to
    # prompt again in that case for every upgrade.
    if [ -d "$daemonResPath" ]; then
        if [ -f "$daemonResPath/architecture.txt" ]; then
            INSTALLED_PIA_ARCHITECTURE="$(cat "$daemonResPath/architecture.txt")"
        else
            # Prior versions of PIA didn't have architecture.txt, and only
            # x86_64 builds were available for Linux.
            INSTALLED_PIA_ARCHITECTURE=x86_64
        fi
    else
        # PIA is not installed
        INSTALLED_PIA_ARCHITECTURE=
    fi

    # If the user has forced the architecture, skip all architecture chceks
    if [ -n "$FORCE_ARCHITECTURE" ]; then
        true # Nothing to check
    # If the architecture being installed is already installed, then still test
    # the new build for compatibility, but handle the results differently.
    elif [ "$INSTALLED_PIA_ARCHITECTURE" == "$buildArchitecture" ]; then
        # This architecture is already installed (assume the user does want this
        # architecture, if it is not an exact match then the user overrode it
        # for the previous install.)
        #
        # Still check that the executables can be executed; library updates may
        # prevent a newer build from running on an older operating system -
        # don't blindly install in that case.
        if ! isBuildExecutable; then
            promptIncompatibleUpgrade
            exit 1
        fi
    # Otherwise, check that this build is the correct architecture.
    else
        # Check if this build can be executed on the host machine - make sure the
        # user isn't trying to install a build for the wrong architecture.  Note
        # that we haven't installed any extra dependency libraries yet, but that
        # piactl --version does not require any of them.
        #
        # This is the preferred way to detect architecture support rather than
        # trying to match uname -m.  There could be variants or changes in uname -m,
        # or future Linux architectures might gain multiarch support (like how amd64
        # is able to execute i686 binaries), etc., we can't predict all of those.
        #
        # We do follow this up with an advisory uname -m check to try to identify
        # situations where an emulated build is being used but a native build is
        # available; etc.
        if "$root/piafiles/bin/${brandCode}ctl" --version >/dev/null 2>&1; then
            # The build architecture can be executed, make sure this is the best
            # build (suggest a native build if this build appears to run under
            # emulation)
            checkBestArchitecture
        else
            # The build can't be executed.  Suggest an alternative if possible or
            # suggest to configure emulation.
            checkAlternateArchitecture
        fi
    fi
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
    while [ "$#" -gt 0 ]; do
        case "$1" in
        --sysvinit)
            BOOT_MANAGER=sysvinit
            shift
            ;;
        --systemd)
            BOOT_MANAGER=systemd
            shift
            ;;
        --openrc)
            BOOT_MANAGER=openrc
            shift
            ;;
        --skip-service)
            BOOT_MANAGER=none
            shift
            ;;
        --force-architecture)
            FORCE_ARCHITECTURE=1
            shift
            ;;
        -h|--help)
            echo "Usage: $brandCode-linux-<version>.run -- <install-options>"
            echo "Install options:"
            echo "  Init system (default is to auto-detect):"
            echo "    --systemd - setup a systemd service on boot"
            echo "    --sysvinit - setup a sysvinit service on boot"
            echo "    --skip-service - skip setting up a service"
            echo "  Architecture (default is to check host architecture)"
            echo "    --force-architecture - install even if build does not match system architecture"
            exit 0
            ;;
        *)
            echo "Unrecognized option: '$1'"
            echo "Type: 'pia-linux-<version>.run -- -h' to see the available options."
            exit 1
            ;;
        esac
    done
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
processOpts "$@"
checkArchitectureSupport
if [ -z "$BOOT_MANAGER" ]; then
    autoDetectSystem
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

# If the installing user has a .pia-early-debug file, create .pia-early-debug in
# the daemon data directory, so it will enable tracing early in startup
if [ -e "$HOME/.$brandCode-early-debug" ]; then
    sudo touch "$daemonDataPath/.$brandCode-early-debug"
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
