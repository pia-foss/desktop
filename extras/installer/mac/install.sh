#!/bin/bash

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

set -e

# Overwrite PATH with known safe defaults
export PATH="/usr/bin:/usr/sbin:/bin:/sbin"

appDir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
logFile="/dev/null"

readonly INSTALLING_USER=${INSTALLING_USER:-$USER}

# Don't use appDir in any of these paths; the uninstaller may run from a
# directory other than the actual app bundle
readonly appName="{{PIA_PRODUCT_NAME}}"
readonly brandCode="{{BRAND_CODE}}"
readonly brandIdentifier="{{BRAND_IDENTIFIER}}"
readonly installDir="/Applications/$appName.app"
readonly bundleId="$brandIdentifier"
readonly launchDaemonPlist="/Library/LaunchDaemons/$brandIdentifier.daemon.plist"
readonly installHelperPlist="/Library/LaunchDaemons/$brandIdentifier.installhelper.plist"
readonly installHelper="/Library/PrivilegedHelperTools/$brandIdentifier.installhelper"
readonly daemonSettingsPath="/Library/Preferences/$bundleId"
readonly oldSettingsPath="/Users/$INSTALLING_USER/.pia_manager/data"
readonly groupName="${brandCode}vpn"
readonly hnsdGroupName="${brandCode}hnsd"         # The group used by the Handshake DNS service
readonly everyoneGUID="ABCDEFAB-CDEF-ABCD-EFAB-CDEF0000000C"
readonly ctlExecutableName="{{BRAND_CODE}}ctl"
readonly ctlExecutablePath="${installDir}/Contents/MacOS/${ctlExecutableName}"
readonly ctlSymlinkDir="/usr/local/bin"
readonly ctlSymlinkPath="${ctlSymlinkDir}/${ctlExecutableName}"

readonly daemonExecutable="$brandCode-daemon"

function enableLogging() {
    logFile="/Library/Application Support/$brandIdentifier/install.log"
    mkdir -p "$(dirname "$logFile")" || true
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
function silent_pfctl() {
    local output="$(pfctl -q "$@" 2>&1)"
    local result=$?
    echo "$output" | grep -vFf <(cat <<EOF
pfctl: Use of -f option, could result in flushing of rules
present in the main ruleset added by the system at startup.
See /etc/pf.conf for further details.
EOF
    ) >> "$logFile"
    return $result
}

# dialog and dialogEx can only be used from the modes that are always run by the
# client in the user session - they can't be used from the "install" and
# "uninstall" modes, which are run by the install helper.
function dialog() {
    local text="$1"
    shift
    osascript > /dev/null 2> /dev/null <<EOF
set piaIcon to (POSIX file "$appDir/Contents/Resources/app.icns") as Unicode text
display dialog "$text" with icon file piaIcon with title "$appName" $@
EOF
}
function dialogEx() {
    local extra="$(cat)"
    dialog "$@" $extra
}

function removeGroups() {
    for group in "$@"; do
        if dscl . -read "/Groups/$group" > /dev/null 2> /dev/null ; then
            dscl . -delete "/Groups/$group" 2>> "$logFile" || true
            echoPass "Deleted group '$group'"
        fi
        # flush the cache
        dscacheutil -flushcache
    done
    true
}

function addGroups() {
    local groupID
    for group in "$@"; do
        # Create the group
        if ! dscl . -read "/Groups/$group" > /dev/null 2>> "$logFile" ; then
            echo "Attempting to add group $group"
            # Find an available group ID
            groupID=$(dscl . -list /Groups PrimaryGroupID 2>> "$logFile" | awk '{ g[$2]=1; } END { for (i=333;i<500;i++) { if(!g[i]) { print i; exit 0; } } exit 1; }') || fail "Failed to find available group ID"
            # Create group
            dscl . -create "/Groups/$group" gid "$groupID" 2>> "$logFile" || fail "Failed to create group '$group'"
            echoPass "Created group '$group' with group ID $groupID"
        fi
        # flush the cache
        dscacheutil -flushcache
    done
    true
}

# Remove the install helper; used by both install and uninstall
function removeInstallHelper() {
    if [ -f "$installHelperPlist" ]; then
        if rm -rf "$installHelperPlist" 2>> "$logFile" &&
             rm -rf "$installHelper" 2>> "$logFile"; then
            echoPass "Removed install helper"
        else
            echoFail "Failed to remove install helper"
        fi
    fi
}

if [[ "$1" == "check" ]] ; then ################################################

    result=0

    # Test if we're in the global Applications directory
    if [ -d "$installDir" ] && ! [[ "$appDir/" == "$installDir/"* ]] ; then
        echoFail "Another installation found in /Applications"
        result=2
    elif [[ "$appDir" == "$installDir" ]] ; then
        echoPass "Installed in /Applications"
    elif [[ "$appDir" == "/Applications/"* ]] ; then
        echoFail "Installed in /Applications but with wrong name"
        result=2
    else
        echoFail "Not installed in /Applications"
        result=2
    fi

    # Test if the group is correctly set up
    groupID=0
    groupHasEveryone=false
    while IFS= read line ; do
        if [[ $line == "PrimaryGroupID:"* ]] ; then
            groupID="$(echo "$line" | cut -d' ' -f2-)"
        elif [[ $line == "NestedGroups:"* && $line == *"$everyoneGUID"* ]] ; then
            groupHasEveryone=true
        fi
    done <<< "$(dscl . -read "/Groups/$groupName" 2> /dev/null)"
    if [[ $groupID == 0 ]] ; then
        echoFail "Group '$groupName' does not exist"
        result=1
    else
        echoPass "Group '$groupName' exists with group ID $groupID"
    fi
    if "$groupHasEveryone" ; then
        echoPass "Group '$groupName' contains nested group 'everyone'"
    else
        echoFail "Group '$groupName' does not contain nested group 'everyone'"
        result=1
    fi

    # Check if client binary has the setgid bit and the right group ID
    #clientGroup="$(stat -f '%Sg' "$appDir/Contents/MacOS/$appName")"
    #if [[ "$clientGroup" == "$groupName" ]] ; then
    #    echoPass "Client binary has owner group '$groupName'"
    #else
    #    echoFail "Client binary has owner group '$clientGroup'"
    #    result=1
    #fi
    #if [ -g "$appDir/Contents/MacOS/$appName" ] ; then
    #    echoPass "Client binary has the sgid flag set"
    #else
    #    echoFail "Client binary does not have the sgid flag set"
    #    result=1
    #fi

    # Check if the daemon is installed
    if [ -f "$launchDaemonPlist" ] ; then
        echoPass "Daemon is registered as a LaunchDaemon"
        if grep -qsF "$appDir/Contents/MacOS/$daemonExecutable" <<< "$(plutil -p "$launchDaemonPlist")" ; then
            echoPass "LaunchDaemon points to the correct $daemonExecutable executable"
        else
            echoFail "LaunchDaemon points to a different $daemonExecutable executable"
            result=1
        fi
    else
        echoFail "Daemon is not registered as a LaunchDaemon"
        result=1
    fi

    exit "$result"

elif [[ "$1" == "check-legacy-upgrade" ]]; then
    # This mode always runs from a user session; we can show dialogs here.
    # ("install" and "uninstall" cannot show dialogs because they are run by the
    # install helper).
    if [ "$brandCode" = "pia" ] && [ -d "$installDir" ] && grep -qF "com.privateinternetaccess.osx.PIA-VPN" "$installDir/Contents/Info.plist" ; then
        # Give the user a chance to abort
        dialogEx "This will replace your existing installation of $appName. Downgrading afterwards will require a clean reinstall. Do you wish to continue?" <<EOF || exit 1
            buttons {"Don't Continue", "Continue"} default button "Continue" cancel button "Don't Continue"
EOF
        # Block as long as the old PIA is running
        while pgrep -q -f "^/Applications/Private Internet Access.app/" ; do
            dialogEx "Please exit the existing $appName before proceeding." <<EOF || exit 1
                buttons {"Cancel", "Try Again"} default button "Try Again" cancel button "Cancel"
EOF
            sleep 1.0
        done
    fi

elif [[ "$1" == "show-install-failure" ]]; then
    dialog "Error: Installation failed.\n\nContact support if this problem reoccurs." 'buttons {"Exit"} default button "Exit"'

elif [[ "$1" == "uninstall" ]] ; then ##########################################

    [[ $EUID -eq 0 ]] || fail "Not running as root"

    enableLogging

    # If a temporary directory was passed, remove it.  The install helper passes
    # the temp directory containing this script here so it will be deleted after
    # the uninstall has started.
    TEMPDIR="$2"
    if [ -n "$TEMPDIR" ]; then
        # Failure ignored; would just leave temporary directory around
        rm -rf "$TEMPDIR" 2>> "$logFile" || true
    fi

    # Stop the daemon
    if pgrep -q "$daemonExecutable" ; then
        [ -f "$launchDaemonPlist" ] && launchctl unload "$launchDaemonPlist" 2>> "$logFile" || true
        pkill "$daemonExecutable" 2>> "$logFile" || true
        echoPass "Stopped daemon"
    fi

    # Remove the piactl symlink, as long as it points to the correct piactl
    # (this is very paranoid since it's in /usr/bin)
    existingSymlinkTarget=$(readlink "${ctlSymlinkPath}" || true)
    if [ "${existingSymlinkTarget}" = "${ctlExecutablePath}" ]; then
        rm "${ctlSymlinkPath}"
        # /usr/local/bin is not deleted
    fi

    # Uninstall the daemon
    if [ -f "$launchDaemonPlist" ] ; then
        rm -rf "$launchDaemonPlist" 2>> "$logFile" || echoFail "Failed to uninstall LaunchDaemon"
        echoPass "Uninstalled LaunchDaemon"
    fi

    # Remove the install helper
    removeInstallHelper

    # Stop any other processes (TODO: softer kill for clients?)
    killedProcesses=false
    while pkill -f "^$installDir/Contents/MacOS/" 2>> "$logFile" ; do
        killedProcesses=true
        sleep 0.5
    done
    if "$killedProcesses" ; then
        echoPass "Killed running processes"
    fi

    # Delete app files
    rm -rf "$installDir" 2>> "$logFile" || echoFail "Failed to remove app directory"
    echoPass "Removed app directory $installDir"

    # Clean up PF if needed
    if grep -q -F "$brandIdentifier" /etc/pf.conf ; then
        cat /etc/pf.conf | grep -v -F "$brandIdentifier" > /etc/pf.conf.new && mv -f /etc/pf.conf.new /etc/pf.conf && silent_pfctl -F rules -f /etc/pf.conf && echoPass "Uninstalled PF anchors" || echoFail "Failed to remove PF anchors"
    fi

    removeGroups $groupName $hnsdGroupName

    # Remove client data
    rm -rf /Users/*/Library/Application\ Support/"$brandIdentifier"
    rm -rf /Users/*/Library/Preferences/"$brandIdentifier"
    rm -f /Users/*/Library/Preferences/"$brandIdentifier.plist"
    rm -f /Users/*/Library/LaunchAgents/"$brandIdentifier.client.plist"

    # Remove daemon data
    rm -rf /Library/Application\ Support/"$brandIdentifier"
    rm -rf /Library/Preferences/"$brandIdentifier"

    exit 0

elif [[ "$1" == "scheduleuninstall" ]] ; then ##################################

    [[ $EUID -eq 0 ]] || fail "Not running as root"

    PID="$2"
    TEMPDIR="$3"

    # If a PID was passed, kill our parent and relaunch
    if [ -n "$PID" ] && [[ $PID =~ ^[0-9]+$ ]] ; then
        # Launch a disowned subshell
        (
            # Wait for parent process to die
            while kill -0 "$PID" 2> /dev/null ; do sleep 0.2 ; done
            # Execute real uninstall script
            exec "$0" uninstall "$TEMPDIR"
        ) & disown
    else
        fail "No PID passed"
    fi

    exit 0

else ###########################################################################

    [[ $EUID -eq 0 ]] || fail "Not running as root"

    enableLogging

    # If a caller PID is passed, that PID is not killed even if it is running
    # from the target directory (needed for reinstall to avoid trying to kill
    # the client that will relaunch after install completes).
    # $1 should be set to "install" to pass a PID as $2, for historical reasons
    # "install" is the default action for any $1
    CALLING_PID="$2"

    echoPass "Running from $appDir, called by PID $CALLING_PID"
    [[ -z "${UNTRANSLOCATED_PATH}" ]] || echoPass "Translocated from ${UNTRANSLOCATED_PATH}"

    # Check if the old PIA client is installed
    if [ "$brandCode" = "pia" ] && [ -d "$installDir" ] && grep -qF "com.privateinternetaccess.osx.PIA-VPN" "$installDir/Contents/Info.plist" ; then
        # Abort if the old PIA is running.  The client does a more user-friendly
        # check before starting the installer script, but we should still check
        # here in case the installer is run manually.
        if pgrep -q -f "^/Applications/Private Internet Access.app/" ; then
            fail "Please exit the existing $appName before installing this update."
        fi

        # Remove old app
        rm -rf "$installDir" || fail "Failed to remove old installation"
        echoPass "Removed old installation"

        # Import settings from old app if possible
        [ "$(cd "/Users/$INSTALLING_USER/.." && pwd)" == "/Users" ] && \
        [ -f "$oldSettingsPath/settings.json" ] && \
        [ ! -f "$daemonSettingsPath/settings.json" ] && \
        mkdir -p "$daemonSettingsPath" && \
        echo "{\"legacy\":$(cat "$oldSettingsPath/settings.json")}" > "$daemonSettingsPath/settings.json" && \
        echoPass "Migrated old settings" || \
        echoFail "Old settings not migrated"

        # Remove old app settings
        rm -rf /Users/*/.pia_manager /Library/Application\ Support/PrivateInternetAccess
    fi

    # Remove the install helper, otherwise an unprivileged process could issue
    # a reinstall/uninstall request at any time.  Uninstalling will elevate
    # again to reinstall the helper.
    removeInstallHelper

    # add the vpn and hnsd groups
    addGroups $groupName $hnsdGroupName

    # Add 'everyone' to our piavpn group as a nested group, otherwise users will not
    # be able to run binaries with the +setgid bit owned by our group on their
    # desktop (note that this does not exempt everyone's traffic)
    if ! dscl . -read "/Groups/$groupName" NestedGroups 2>> "$logFile" | grep -qF "$everyoneGUID" ; then
        dseditgroup -o edit -a everyone -t group "$groupName" || fail "Failed to add 'everyone' to group '$groupName'"
        echoPass "Added 'everyone' to group '$groupName'"
    fi

    # Help flush group changes so they're visible to other posix commands
    dscacheutil -flushcache 2>> "$logFile" || true
    id > /dev/null 2>> "$logFile" || true

    # Stop daemon if it is running
    if pgrep "$daemonExecutable"; then
        [ -f "$launchDaemonPlist" ] && launchctl unload "$launchDaemonPlist" 2>> "$logFile" || true
        pkill "$daemonExecutable" 2>> "$logFile" || true
        echoPass "Stopped daemon"
    else
        [ -f "$launchDaemonPlist" ] && launchctl unload "$launchDaemonPlist" 2>> "$logFile" || true
    fi

    # Reload the PF rules
    silent_pfctl -f /etc/pf.conf || true

    # Quietly remove old piaX files if present
    if [ -d "/Applications/piaX.app" ] && grep -qF "com.privateinternetaccess.vpn" "/Applications/piaX.app/Contents/Info.plist" 2> /dev/null ; then
        while pkill -f "^/Applications/piaX.app/Contents/MacOS/" 2>> "$logFile" ; do
            sleep 0.5
        done
        rm -rf "/Applications/piaX.app"
        echoPass "Removed alpha version"
    fi

    # Move to /Applications if necessary
    if ! [[ "$appDir" == "$installDir" ]] ; then

        # Kill any existing processes running from the destination directory,
        # except for CALLING_PID
        killedProcesses=false
        searchProcesses=true
        while "$searchProcesses"; do
            searchProcesses=false
            for pid in $(pgrep -f "^$installDir/Contents/MacOS/" 2>> "$logFile"); do
                if [ -z "$CALLING_PID" ] || [ "$pid" -ne "$CALLING_PID" ]; then
                    searchProcesses=true
                    killedProcesses=true
                    # Failure ignored, process could have died already
                    ps -p "$pid" >> "$logFile" || true
                    kill "$pid" 2>> "$logFile" || true
                fi
            done
        done

        if "$killedProcesses" ; then
            echoPass "Killed running processes"
        fi

        # Delete the destination directory if it exists
        if [ -d "$installDir" ] ; then
            rm -rf "$installDir" 2>> "$logFile" || fail "Failed to delete existing installation"
            echoPass "Deleted existing installation"
        fi

        # If the destination directory isn't writable even though we're root,
        # we're probably running under app translocation
        if [ -w "$appDir" ] ; then
            mv -f "$appDir" "$installDir" 2>> "$logFile" || fail "Failed to move app to /Applications"
            echoPass "Moved app to /Applications"
        else
            # Copying seems to still work under app translocation
            cp -fpR "$appDir" "$installDir" 2>> "$logFile" || fail "Failed to copy app to /Applications"
            echoPass "Copied app to /Applications"
            # If we've been passed our own untranslocated path via the environment, remove it
            if [[ ! -z "${UNTRANSLOCATED_PATH}" ]]; then
                # To be on the safe side, only remove paths under /Users/
                if [[ "${UNTRANSLOCATED_PATH}" == "/Users/"* ]]; then
                    rm -rf "${UNTRANSLOCATED_PATH}" && echoPass "Removed original app location" || echoFail "Failed to remove ${UNTRANSLOCATED_PATH}"
                else
                    echoPass "Leaving original app location in place"
                fi
            else
                # As a fallback, use the original applescript approach
                osascript -e "set f to POSIX file \"$appDir\"" -e 'tell application "Finder" to move f to trash' 2>> "$logFile" || echoFail "Unable to remove original app location"
            fi
        fi
        # Try to coax Spotlight into finding the new app as soon as possible
        mdimport "$installDir" > /dev/null 2> /dev/null || true

        # Update appDir to new directory
        appDir="$installDir"

    fi

    # Unquarantine app if necessary
    #if xattr "$appDir" | grep -qxF com.apple.quarantine ; then
        "$appDir/Contents/MacOS/$brandCode-unquarantine" "$appDir" 2>> "$logFile" || fail "Failed to unquarantine app bundle"
        echoPass "Unquarantined app bundle"
    #fi

    # Fix permissions (root owner, group+sgid)
    chown -R root:wheel "$appDir" 2>> "$logFile" || fail "Failed to set app ownership"
    #chgrp "$groupName" "$appDir/Contents/MacOS/$appName" 2>> "$logFile" || fail "Failed to set group owner on client executable"
    #chmod g+s "$appDir/Contents/MacOS/$appName" 2>> "$logFile" || fail "Failed to set the sgid bit on client executable"
    echoPass "Updated permissions"

    # Install daemon
    cat <<EOF > "$launchDaemonPlist" 2>> "$logFile" || fail "Failed to install LaunchDaemon"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple/DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>$brandIdentifier.daemon</string>
    <key>ProgramArguments</key>
    <array>
        <string>$appDir/Contents/MacOS/$daemonExecutable</string>
    </array>
    <key>KeepAlive</key>
    <true/>
    <key>Umask</key>
    <integer>022</integer>
    <key>EnableTransactions</key>
    <false/>
</dict>
</plist>
EOF
    echoPass "Installed LaunchDaemon"

    # If the /usr/local/bin/piactl symlink doesn't exist at all, link it here.
    # (If it exists, either it's already ours and doesn't need to be updated, or
    # it's something else and we shouldn't touch it.)  Note that -e returns
    # false for broken symlinks, so test both -L and -e.
    # If this fails, ignore it - it should work, but it's not required to use
    # PIA.
    if [ ! -L "${ctlSymlinkPath}" ] && [ ! -e "${ctlSymlinkPath}" ]; then
        # Make /usr/local/bin if it doesn't already exist (it is in PATH by
        # default, but the directory doesn't actually exist by default)
        mkdir -p "${ctlSymlinkDir}"
        ln -s "${ctlExecutablePath}" "${ctlSymlinkPath}"
    fi

    # If the installing user has a .pia-early-debug file, create a corresponding
    # file in the daemon data directory, so it will enable tracing early in
    # startup.
    if [ -e "/Users/$INSTALLING_USER/.$brandCode-early-debug" ]; then
        touch "/Library/Application Support/$brandIdentifier/.$brandCode-early-debug"
    fi

    # Launch daemon
    launchctl load -w "$launchDaemonPlist" 2>> "$logFile" || fail "Failed to launch daemon"
    for i in $(seq 1 25) ; do
        if pgrep -q "$daemonExecutable" ; then
            break
        fi
        sleep 0.2
    done
    if pgrep -q "$daemonExecutable" ; then
        echoPass "Launched daemon"
    else
        fail "Failed to launch daemon (timed out)"
    fi

    exit 0

fi #############################################################################
