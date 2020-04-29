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

printHelp() {
  echo '''
    Required environment variables:
    OVPN_SERVER_IP
    OVPN_SERVER_NAME
    OVPN_USERNAME
    OVPN_PASSWORD
    OVPN_PROTO=[udp|tcp]

    Optional environment variables:
    OVPN_PORT
  '''
  exit 1
}
if [ -z "$OVPN_SERVER_IP" ] || [ -z "$OVPN_PASSWORD" ] || [ -z "$OVPN_SERVER_NAME" ] || [ -z "$OVPN_USERNAME" ] || [ -z "$OVPN_PROTO" ]; then
  printHelp
fi

if [ -z "$OVPN_PORT" ]; then
  case "$OVPN_PROTO" in
  "udp") OVPN_PORT="8080" ;;
  "tcp") OVPN_PORT="500" ;;
  *) printHelp ;;
  esac
fi

cd /root || exit 1
mkdir -p /dev/net
mknod /dev/net/tun c 10 200
chmod 666 /dev/net/tun
echo "$OVPN_USERNAME" > "auth.txt"
echo "$OVPN_PASSWORD" >> "auth.txt"
cp "/root/openvpn_$OVPN_PROTO.ovpn" openvpn.ovpn
sed -i "s/SERVER_IP/$OVPN_SERVER_IP/" openvpn.ovpn
sed -i "s/SERVER_NAME/$OVPN_SERVER_NAME/" openvpn.ovpn
sed -i "s/SERVER_PORT/$OVPN_PORT/" openvpn.ovpn
/root/pia-openvpn/bin/pia-openvpn openvpn.ovpn
