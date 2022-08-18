# Building Desktop Dependencies

This directory contains external dependencies built in the [desktop-dep-build](https://github.com/pia-foss/desktop-dep-build) repository.  These are all third party components; in some cases slight modifications have been applied for PIA.

Precompiled binaries are included here to make it easier to get started with development on PIA Desktop.  See [desktop-dep-build/README.md](https://github.com/pia-foss/desktop-dep-build/blob/master/README.md) for instructions to build these dependencies, and refer to [build.txt](build.txt) for the commit used in this build.

# Components

The following components are included:

* OpenSSL (libcrypto/libssl)
* OpenVPN (pia-openvpn)
  * Additional libraries on Windows for OpenVPN - lzo2, libpkcs11-helper-1
* hnsd (pia-hnsd, no longer shipped since 2.2)
* Unbound (pia-unbound)
* Shadowsocks local client (pia-ss-local)
* WireGuard userspace implementation (Windows - pia-wgservice, Mac/Linux - pia-wireguard-go)
  * pia-wgservice is based on embeddable-dll-service from the WireGuard project, by Jason A. Donenfeld.
