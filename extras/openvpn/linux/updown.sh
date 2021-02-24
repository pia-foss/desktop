#!/bin/bash -e

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

function warn() {
  local status=$?
  [ $status -eq 0 ] && echo "$@" >&2 || echo "$@" "($status)" >&2
}

function dns_error() {
  warn "$@"

  # magic string that indicates a DNS error, this is picked up by openvpn management
  # code and results in a client-side error for the user.
  local MAGIC_STRING_DNS_ERROR="!!!updown.sh!!!dnsConfigFailure"
  echo $MAGIC_STRING_DNS_ERROR >&2
  exit 1
}

function systemd_apply() {
  echo "Applying nameservers using systemd..."
  local tunnel_interface_index="$(ip -o link | awk -F': ' '$2=="'"$dev"'" { print $1; exit }')"

  [ -n "$tunnel_interface_index" ] || dns_error "Unable to identify tunnel interface"
  echo "Device $dev = index $tunnel_interface_index"

  # Set DNS servers on the tunnel interface.
  /usr/bin/busctl call org.freedesktop.resolve1 /org/freedesktop/resolve1 org.freedesktop.resolve1.Manager \
    SetLinkDNS 'ia(iay)' \
    $tunnel_interface_index \
    $(echo $dns_servers | wc -w) \
    $(echo $dns_servers | sed 's/[^ ]* */2 4 &/g' | tr '.' ' ') \
    || dns_error "Failed to call SetLinkDNS on interface"

  # Force tunnel interface to handle all DNS queries.
  /usr/bin/busctl call org.freedesktop.resolve1 /org/freedesktop/resolve1 org.freedesktop.resolve1.Manager \
    SetLinkDomains 'ia(sb)' \
    $tunnel_interface_index \
    1 \
    '.' 'true' \
    || dns_error "Failed to call SetLinkDomains on interface"
}

function resolvconf_apply() {
  echo "Applying nameservers using resolvconf..."

  # using 169.254.12.97 is a dummy (link-local) address as ubuntu 16.04 resolvconf needs 3 ip addresses or it falls back to the original
  # user dns config as the 3rd ip, which could cause leaks
  local final_dns_servers=$(echo $dns_servers 169.254.12.97 169.254.12.98 | xargs -n1 echo nameserver | head -n 3)

  echo "Updating resolvconf with:"
  echo $final_dns_servers
  # Hard-code tun0.* rather than use ${dev}.openvpn (which we used previously)
  # this is so we can ensure high priority as per /run/resolvconf/interface-order
  # Wireguard interfaces (wg*) do not appear in this file and
  # would have lowest priority (if we had used ${dev}.openvpn) meaning our chosen DNS might not be set at all
  echo $final_dns_servers | resolvconf -a "tun0.{{BRAND_CODE}}" || dns_error "Failed to reconfigure using resolvconf"
}

[ -n "$script_type" ] || dns_error "Missing script_type env var"
[ -n "$dev" ] || dns_error "Missing dev env var"

dns_servers=""
domain=""
pia_args_done=""

# OpenVPN discards PATH when invoking this script, due to setting up a
# "clean" environment and then applying its own variables.  This seems to
# be an oversight, but it prevents us from finding executables like 'ip'
# when they aren't in the default PATH that bash guesses.
#
# We can't easily set variables in the script environment through
# OpenVPN's interface, so instead this is explicitly passed to the script
# as an argument.
#
# On top of all that, OpenVPN also limits args to 256 chars for some reason
# (options.h, OPTION_PARM_SIZE), so we have to split up PATH if it is longer
# than that (which has been encountered in the field).
arg_path=""

while [ "$#" -gt 0 ] && [ -z "$pia_args_done" ]; do
  case "$1" in
    "--dns")
      shift
      dns_servers="$(echo $1 | tr : \ )"
      ;;
    "--path")
      shift
      # Append this segment to arg_path, long PATH values have to be split up to
      # work around OpenVPN limitations.  Then the whole path is applied later.
      arg_path="$arg_path$1"
      ;;
    "--")
      pia_args_done=1
      ;;
    *)
      echo "Unknown option $1"
      shift
      ;;
  esac
  shift
done

if [ -n "$arg_path" ]; then
  echo "PATH provided from command line: $arg_path"
  export PATH="$arg_path"
fi

for (( i=1 ; ; i++ )); do
  var="foreign_option_$i"
  value="${!var}"
  [ -n "$value" ] || break
  case "$value" in
    "dhcp-option DNS "*)
      dns_servers="$(echo $dns_servers "${value##dhcp-option DNS }")"
    ;;
    "dhcp-option DOMAIN "*)
      domain="${value##dhcp-option DOMAIN }"
    ;;
  esac
done

resolv_conf_backup=/opt/{{BRAND_CODE}}vpn/var/pia.resolv.conf
resolvconf_link_path=/run/resolvconf/resolv.conf

case "$script_type" in
  up)
    echo "Setting up DNS."
    echo "Using device:$dev local_address:$ifconfig_local remote_address:$route_vpn_gateway" >&2 # Used in openvpnmethod.cpp to pass in tunnel info

    if [ -n "$dns_servers" ] ; then
      echo "Requested nameservers: $dns_servers"

      # If the symlink target has "systemd" in the path then assume systemd-resolve is in control
      if [[ $(realpath /etc/resolv.conf) =~ systemd ]]; then
        systemd_apply

      # There's many possible ways resolvconf can be used, but we only support it when there's a symlink to /run/resolvconf
      # All other possibilites fall back to a manual overwrite of /etc/resolv.conf
      elif [ $(realpath /etc/resolv.conf) = $resolvconf_link_path ] && hash resolvconf 2> /dev/null; then
        resolvconf_apply

      # check whether immutable bit is set
      elif lsattr /etc/resolv.conf 2> /dev/null | grep -q i; then
        dns_error "Failed to update DNS servers: /etc/resolv.conf was set +i (immutable)"
      else
        echo "Applying nameservers by overwriting resolv.conf..."
        echo "Backing up /etc/resolv.conf to $resolv_conf_backup"
        # let's overwrite /etc/resolv.conf, but let's back it up first
        # also let's only back it up if we don't already have a backup, otherwise
        # we may replace the previously backed-up config, which may be the only
        # copy remaining if for example their computer reset without us restoring
        # the backup previously.
        [ ! -f $resolv_conf_backup ] && cat /etc/resolv.conf > $resolv_conf_backup && sync $resolv_conf_backup

        # now let's write our nameservers to /etc/resolv.conf
        echo $dns_servers | xargs -n1 echo nameserver > /etc/resolv.conf
      fi
  fi

  ;;

  down)
    echo "Tearing down DNS."
    if [[ $(realpath /etc/resolv.conf) =~ systemd ]]; then
      # Nothing to do
      true
    elif  [ $(realpath /etc/resolv.conf) = $resolvconf_link_path ] && hash resolvconf 2>/dev/null; then
      echo "Resetting resolvconf configuration..."
      # Hard-code tun0.* rather than use ${dev}.openvpn (which we used previously)
      # see comment above in resolvconf_apply
      resolvconf -d "tun0.{{BRAND_CODE}}" || warn "Failed to reset resolvconf"
    else
      if [ -e $resolv_conf_backup ]; then
        echo "Restoring user /etc/resolv.conf from backup"
        cat $resolv_conf_backup > /etc/resolv.conf && rm $resolv_conf_backup
      fi
    fi
  ;;
esac
