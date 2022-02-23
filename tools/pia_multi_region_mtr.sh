#! /usr/bin/env bash

# Copyright (c) 2022 Private Internet Access, Inc.
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

# This is the list of servers to test - needs both CNs and IPs.  Each pair of
# elements represents a server.
SERVERS=( \
 sydney408 117.120.10.37 \
 sydney410 117.120.10.70 \
 sydney407 117.120.10.5 \
 melbourne404 103.2.198.83 \
 melbourne406 27.50.74.157 \
 toronto422 154.3.42.15 \
 toronto423 154.3.42.55 \
)

# sydney408 117.120.10.37
# sydney410 117.120.10.70
# sydney415 27.50.76.116
# sydney407 117.120.10.5
# sydney401 103.2.196.170

# melbourne404 103.2.198.83
# melbourne406 27.50.74.157
# melbourne403 103.2.198.75
# melbourne412 103.2.198.119
# melbourne411 43.242.68.226

# toronto422 154.3.42.15
# toronto423 154.3.42.55
# toronto418 154.3.40.146
# toronto402 66.115.142.9
# toronto405 172.83.47.223

die() { echo "$*" 1>&2 ; exit 1; }

# Select a value for either Linux ($1) or macOS ($2)
function select_platform(){
    case "$(uname)" in
        Linux)
            echo "$1"
            ;;
        Darwin)
            echo "$2"
            ;;
        *)
            die "Unsupported platform: $(uname)"
            ;;
    esac
}

function platform() {
    select_platform "linux" "macos"
}

MTR="$(which mtr)"
# On macOS mtr is typically installed from Homebrew and can't be found in PATH,
# that's the default macOS path here.
MTR="${MTR:-$(select_platform "/usr/sbin/mtr" "/usr/local/sbin/mtr")}"

LOG="$HOME/pia_mtr_results.log"

if ! [ -x "$MTR" ]; then
    echo "Couldn't find mtr executable."
    if [ "$(platform)" = "linux" ]; then
        echo "Install mtr from your package manager, then run this script again."
    else
        echo "Install mtr using the instructions here:"
        echo "  https://www.privateinternetaccess.com/helpdesk/guides/mac/troubleshooting-3/mtr-speed-test-2"
    fi
    exit 1
fi

# mtr requires root on macOS
if [ "$(platform)" = "macos" ]; then
    if [[ $EUID -ne 0 ]]; then
        echo "MTR requires root on macOS"
        # Capture the current user ID and re-invoke as root, this lets the
        # script set the log file's owner to the user
        sudo bash "$0" --for-user "$USER"
        exit 0
    fi
fi

FOR_USER=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --for-user)
            FOR_USER="$2"
            shift
            shift
            ;;
        *)
            echo "Unrecognized option: $1"
            exit 1
            ;;
    esac
done

# Just write something to the log
function log() {
    echo "$(date -u "+%Y-%m-%d %H:%M:%S.%s")" "$*" >>"$LOG"
}
# Write to output and to the log
function print() {
    log "$*"
    echo "$*"
}

{ echo ""; echo ""; echo ""; } >> "$LOG"
print "Regional MTR test starting"

# If we elevated to root on macOS, make sure the log file is still owned by the
# user
if [ -n "$FOR_USER" ]; then
    chown "$FOR_USER" "$LOG"
fi

piactl set debuglogging true
# Use WireGuard for the test
piactl -u applysettings '{"method":"wireguard"}'
# Select the manual region.  This is fine even if the manual region isn't set
# yet, it's stored but ignored until the manual region is a valid choice
piactl -u applysettings '{"location":"manual"}'
# Enable background mode in case the client is not open
piactl background enable

# Wait about 15 seconds to connect.  This is approximate due to crude "sleep and
# count" delays but OK for this purpose.
function wait_for_state() {
    TARGET="$1"
    local i=0
    while [ "$i" -lt 15 ] && [ "$(piactl get connectionstate)" != "$TARGET" ]; do
        sleep 1
        ((i=i+1))
    done
    if [ "$i" -lt 15 ]; then
        print "Reached state $TARGET after $i seconds"
        return 0
    else
        print "Failed to reach state $TARGET after $i seconds"
        return 1
    fi
}

function log_mtr() {
    HOST="$1"
    # We specify 60 cycles (at 1 sec/cycle), but the whole test actually takes
    # longer, so show 3 minutes to the user.
    print "Testing packet loss to $HOST (3 minutes)..."
    "$MTR" --report --report-cycles 60 "$HOST" >> "$LOG"
    print "Packet loss test to $HOST completed"
}

function test_server() {
    IP="$1"
    CN="$2"
    # First, do an MTR test to the server while disconnected
    print "Beginning test of $CN - $IP"
    log_mtr "$1"
    piactl -u applysettings "{\"manualServer\":{\"ip\":\"$IP\",\"cn\":\"$CN\",\"correspondingRegionId\":\"\",\"serviceGroups\":[]}}"
    print "Connecting to $CN - $IP"
    piactl connect
    if ! wait_for_state "Connected"; then
        print "Couldn't connect to $CN - $IP, skipping this server"
        return 0
    fi
    sleep 5 # Let post-connection traffic settle
    log_mtr "1.1.1.1"
    piactl disconnect
    if ! wait_for_state "Disconnected"; then
        print "Test aborted - unable to disconnect from $CN - $IP"
        print "Please submit a debug report to troubleshoot this failure"
        return 1
    fi
    sleep 5 # Let disconnect cache resets settle
}

# Make sure we're disconnected before starting
piactl disconnect
if ! wait_for_state "Disconnected"; then
    print "Test aborted - unable to disconnect from existing connection"
    print "Please submit a debug report to troubleshoot this failure"
    return 1
fi

SERVER_COUNT=$(( ${#SERVERS[@]} / 2 ))
idx=0
while [ "$idx" -lt "$SERVER_COUNT" ]; do
    cn_idx=$(( idx * 2 ))
    ip_idx=$(( cn_idx + 1 ))
    if ! test_server "${SERVERS[$ip_idx]}" "${SERVERS[$cn_idx]}"; then
        break
    fi
    ((idx=idx+1))
done

# Clean up the settings that were changed.  "method" is left on WireGuard
piactl -u applysettings '{"location":"auto"}'
piactl background disable
piactl -u applysettings '{"manualServer":{"ip":"","cn":"","correspondingRegionId":"","serviceGroups":[]}}'

echo "Test complete!"
echo "Please submit a debug report from PIA, then send the 5-character code and"
echo "the test results file to the development team:"
echo "  $LOG"
