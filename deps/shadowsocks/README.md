# Building the Shadowsocks client (ss-local)

The Private Internet Access desktop client uses a custom build of [shadowsocks-libev](https://github.com/shadowsocks/shadowsocks-libev) with minor patches and static linkage. To build the binaries for each platform, check out our [separate Git repository](https://github.com/pia-foss/desktop-shadowsocks) which also contains the patches applied to shadowsocks-libev.

Builds are performed natively on each platform; instructions are in the `desktop-shadowsocks` repository.

Since building shadowsocks-libev is extra overhead, we also include precompiled binaries in the main repository. This makes it a lot easier to quickly get started with development. See [build.txt](build.txt) for the commit ID used for the current precompiled binaries.
