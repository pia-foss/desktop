# Building the Handshake resolver (hnsd)

The Private Internet Access desktop client uses a custom build of [hnsd](https://github.com/handshake-org/hnsd) with Windows support and static linkage. To build the binaries for each platform, check out our [separate Git repository](https://github.com/pia-foss/desktop-hnsd) which also contains the patches applied to hnsd.

Builds are performed natively on each platform; instructions are in the `desktop-hnsd` repository.

Since building hnsd is extra overhead, we also include precompiled binaries in the main repository. This makes it a lot easier to quickly get started with development. See [build.txt](build.txt) for the commit ID used for the current precompiled binaries.
