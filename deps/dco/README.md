# Building the OpenVPN DCO Driver

The Private Internet Access desktop client uses a custom build of [ovpn-dco-win](https://github.com/OpenVPN/ovpn-dco-win) from the OpenVPN project, renamed and given a unique hardware ID in order to avoid conflicts with other installed applications.

DCO (Data Channel Offload) moves the OpenVPN data channel into a kernel driver, which avoids copying every packet through userspace.  It only supports UDP, and it cannot apply compression framing - see [Windows VPN drivers.md](../../docs/Windows%20VPN%20drivers.md) for how the client selects between DCO, WinTUN, and TAP.

Precompiled and signed versions of the driver are included in this repository. Note however that without a proper digital signature, the drivers cannot be installed on a typical Windows machine.

See [build.txt](build.txt) for the upstream commit ID used for the current precompiled binaries.

The driver name is fixed by its signature: the `.sys`/`.inf`/`.cat` filenames, the service name and hardware ID declared in the INF, and the name the daemon probes for must all agree.  Renaming the driver requires re-signing it.
