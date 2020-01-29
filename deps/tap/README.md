# Building the Windows TAP Adapter

The Private Internet Access desktop client uses a custom build of the [TAP Adapter](https://github.com/OpenVPN/tap-windows6) from the OpenVPN project, renamed and given a unique hardware ID in order to avoid conflicts with other installed applications.

Precompiled and signed versions of the TAP adapter are included in this repository, but you can check out and rebuild the driver yourself from our [separate Git repository](https://github.com/pia-foss/desktop-tap). Note however that without a proper digital signature, the drivers cannot be installed on a typical Windows machine.

See [build.txt](build.txt) for the commit ID used for the current precompiled binaries.

When updating this driver, TAP_DRIVER_VERSION in extras/installer/win/tap.inl must be updated to match the new driver version.
