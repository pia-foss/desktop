# Embeddable Wireguard Library

This is the embeddable Wireguard client library from wireguard-tools, licensed under the LGPL-2.1+.

Original repository: https://git.zx2c4.com/wireguard-tools/

Commit: `8082f7e6a88af9299952c472feae2bb6153dbb6c`

# Modifications

PIA uses the wg_device structures to describe a WireGuard device on all platforms, so minor patches were made to allow wireguard.h to be used on Windows (networking structures are in WinSock headers.)

No changes were made to wireguard.c, which is only used on Linux.

## Original README and license notice

```
Embeddable WireGuard C Library
==============================

This is a mini single-file library, meant to be embedded directly into the
source code of your program. It is *not* meant to be built as a shared
library.


Usage
-----

Copy wireguard.c and wireguard.h into your project. They should build with
any C89 compiler. There are no dependencies except libc.

Please see the set of simple functions in wireguard.h for information on
how to use, as well as the example code in test.c.


License
-------

Because this uses code from libmnl, wireguard.c and wireguard.h are licensed
under the LGPL-2.1+.
```
