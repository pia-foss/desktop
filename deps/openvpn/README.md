# Building OpenVPN

The Private Internet Access desktop client uses a custom build of [OpenVPN](https://github.com/OpenVPN/openvpn) with a few modifications. To build the binaries for each platform, check out our [separate Git repository](https://github.com/pia-foss/desktop-openvpn) which also contains the patch set applied on top of OpenVPN.

Builds are performed natively on each platform; instructions are in the `desktop-openvpn` repository.

Since building OpenVPN is extra overhead, we also include precompiled binaries in the main repository. This makes it a lot easier to quickly get started with development. See [build.txt](build.txt) for the commit ID used for the current precompiled binaries.
