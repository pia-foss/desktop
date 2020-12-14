# PIA chroot build environment

All final artifacts shipped with PIA Desktop are built in a Debian Stretch chroot to maximize compatibility.

Cross builds are also possible - release artifacts for armhf and arm64 are cross-compiled from an x86_64 host

These scripts create and enter a chroot.  setup.sh creates the chroot (must be run as root), and enter.sh can be used to enter the chroot (either as a regular user or root).  This should work on any system with debootstrap and schroot available.

The Qt installation must be in $HOME or /opt to be accessible in the chroot (via bind mounts).  If this repository is not under $HOME, the script will prompt for a bind mount location to make the repo available in the chroot.

# Host architecture builds

The commands are run from the `desktop` repository root in this example, but they can also be run from scripts/chroot, etc.

This works for `amd64`, `arm64`, and `armhf` hosts.  Other architectures can be added by building dependencies from the desktop-dep-build repository.

```shell
# Set up the chroot:
$ ./scripts/chroot/setup.sh  # Prompts for root password
# Enter the chroot:
$ ./scripts/chroot/enter.sh
# Now, build in chroot:
(piabuild-stretch)$ rake all
```

# Cross builds

This example uses `arm64` (64 bit); `armhf` is also supported (32-bit hard float).

PIA's Qt build from desktop-dep-build must be installed for both the host and target architectures (compilation tools are used from the host build, build dependencies are used from the target build).

```shell
# Create a chroot with cross build dependencies:
$ ./scripts/chroot/setup.sh --cross-target arm64
# Enter the chroot:
$ ./scripts/chroot/enter.sh arm64
# Now, build in chroot for arm64 target:
(piabuild-stretch-arm64)$ rake ARCHITECTURE=arm64 all
```

# Dependency artifacts

The external dependencies built by the desktop-dep-build repo are also built in this chroot environment, but this repo is not set up for cross compilation - armhf and arm64 builds must be done natively on an ARM device.
