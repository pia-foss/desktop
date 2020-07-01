# Building the Handshake resolver (pia-hnsd) and Unbound (pia-unbound)

The Private Internet Access desktop client uses [hnsd](https://github.com/handshake-org/hnsd) and [unbound](https://github.com/NLnetLabs/unbound). To build the binaries for each platform, check out our [separate Git repository](https://github.com/pia-foss/desktop-hnsd) which also contains the patches applied.

Builds are performed natively on each platform; instructions are in the `desktop-hnsd` repository.

Since building these is extra overhead, we also include precompiled binaries in the main repository. This makes it a lot easier to quickly get started with development. See [build.txt](build.txt) for the commit ID used for the current precompiled binaries.
