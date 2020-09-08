#! /usr/bin/env bash

if ! [ -d "$1" ]; then
    echo "usage:"
    echo "$0 <path_to_desktop-openvpn_repo>"
    echo ""
    echo "Parameters:"
    echo "  path_to_desktop-openvpn_repo:"
    echo "   Path to the local copy of the desktop-openvpn repo"
    echo "   (https://github.com/pia-foss/desktop-openvpn), which is used"
    echo "   as the OpenSSL dependency when building libk5crypto.so."
    echo ""
    echo "This script builds libk5crypto.so from source for use in PIA."
    echo "Although PIA does not use Kerberos, Qt 5.15 links to it, and"
    echo "the library in some distributions is not compatible with PIA's"
    echo "OpenSSL library (due to extra exported symbols being added to"
    echo "OpenSSL in some distributions."
    exit 1
fi

OPENVPN24_REPO_PATH="$(cd "$1" && pwd)"

SCRIPTROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPTROOT"

rm -rf build-krb5
mkdir -p build-krb5/install
cd build-krb5

curl -O "https://web.mit.edu/kerberos/dist/krb5/1.18/krb5-1.18.2.tar.gz"
tar --gz -xf krb5-1.18.2.tar.gz -C .

pushd krb5-1.18.2/src
# Use OpenSSL 1.1 built by the OpenVPN build scripts
export CPPFLAGS="$CPPFLAGS -I${OPENVPN24_REPO_PATH}/out/build/linux64/openvpn/include"
export LDFLAGS="$LDFLAGS -L${OPENVPN24_REPO_PATH}/out/build/linux64/openvpn/lib"

./configure --prefix="$SCRIPTROOT/build-krb5/install" --with-crypto-impl=openssl --with-tls-impl=openssl
make
make install
popd

cp "$SCRIPTROOT/build-krb5/install/lib/libk5crypto.so"* "$SCRIPTROOT/"

echo "Built libk5crypto.so:"
ls -lh "$SCRIPTROOT/build-krb5/install/lib/libk5crypto.so"*
