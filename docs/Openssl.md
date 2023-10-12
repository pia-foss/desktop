# Openssl dependencies

## General notes

Openssl is composed of two pre-built dynamic libraries:  
*libcrypto* and *libssl*  
libssl depends on libcrypto, while libcrypto has no dependencies.

Currently these are the components that are using openssl libs:
- libcrypto:
  - pia-daemon (called pia-service on Windows)
  - Qt
  - Openvpn

- libssl:
  - Qt
  - Openvpn
  - pia-unbound
  - pia-hnsd

## Historical notes

Here goes an explanation of why we load OpenSSL in runtime instead of in load time
It's unclear whether any of these reasons apply any more, but it isn't completely
uncommon for OpenSSL to be loaded at runtime.
Qt does essentially the same thing, and PIA has always done it this way.
Reasons:
- There was a time historically when we tolerated a failure to load OpenSSL.
For instance, the regions list signature validation would 'fail closed',
and we just didn't validate the signature, this was the only thing at the
time. The only way to do that is to link at runtime.
- That in turn was because we didn't ship our own OpenSSL on macOS (I think
we always did on Win/Linux), and because OpenSSL historically has broken its API
significantly between 0.9/1.0/1.1/.etc., there was the possibility we would run
against a version we didn't expect or not find it at all. So it was tricky to
locate OpenSSL on the system and tricky to know if it was the right version.
- Qt does the same thing, and I think there was a possibility that trying to
link at load time vs. runtime would conflict with Qt loading the lib at runtime.
Today we ship OpenSSL on all platforms and try to ensure we load first and that Qt
loads the same one, which should avoid any conflicts.
*Edit: Qt does actually load from other paths if it can not find libcrypto in the 
apps library path (e.g. /opt/piavpn/lib in Linux)*
- OpenSSL is under a weird license, the SSLeay license (thanks Eric A Young :-/).
Technically we should probably offer the OpenSSL linking exception on the GPL we apply
to PIA, this never occurred to me until now. SSLeay is not GPL-compatible, in my mind
keeping it "as far as possible" from us by runtime linking suggests that it is an
independent component we are driving through an interface, although there is no
case law on this so this may also be nothing useful.

## How different components load Openssl

### Openvpn

Openvpn is loading Openssl dynamically at run-time, but it is linking against it at build-time.  
(Dynamic linking / load-time dynamic linking)  
Inspecting the Openvpn binary can clearly show which library object it was linked against. 

*Linux*:  
checking library names:  
```
$ ldd deps/built/linux/arm64/pia-openvpn 
      linux-vdso.so.1 (0x0000ffff84701000)
      libnsl.so.1 => /lib/aarch64-linux-gnu/libnsl.so.1 (0x0000ffff84290000)
      libresolv.so.2 => /lib/aarch64-linux-gnu/libresolv.so.2 (0x0000ffff84260000)
      libssl.so.1.1 => /lib/aarch64-linux-gnu/libssl.so.1.1 (0x0000ffff841c0000)
      libcrypto.so.1.1 => /lib/aarch64-linux-gnu/libcrypto.so.1.1 (0x0000ffff83f10000)
      libc.so.6 => /lib/aarch64-linux-gnu/libc.so.6 (0x0000ffff83d60000)
      /lib/ld-linux-aarch64.so.1 (0x0000ffff846c4000)
      libpthread.so.0 => /lib/aarch64-linux-gnu/libpthread.so.0 (0x0000ffff83d30000)
      libdl.so.2 => /lib/aarch64-linux-gnu/libdl.so.2 (0x0000ffff83d00000)
```
checking the rpath:  
```
$ readelf -d deps/built/linux/arm64/pia-openvpn 

Dynamic section at offset 0xb0000 contains 31 entries:
  Tag        Type                         Name/Value
 0x000000000000000f (RPATH)              Library rpath: [$ORIGIN/../lib/]
 0x0000000000000001 (NEEDED)             Shared library: [libnsl.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libresolv.so.2]
 0x0000000000000001 (NEEDED)             Shared library: [libssl.so.1.1]
 0x0000000000000001 (NEEDED)             Shared library: [libcrypto.so.1.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 ...
```
another alternative is:
```
$ patchelf --print-rpath deps/built/linux/arm64/pia-openvpn 
$ORIGIN/../lib/
```
`$ORIGIN` means the same folder as the file.  

Be aware that the rpath is just the first folder in which the system will start looking for  
dynamic libs. Refer to *Dynamic libraries.md* doc.

*macOS*:  
```
$ otool -L deps/built/mac/arm64/pia-openvpn 
deps/built/mac/arm64/pia-openvpn:
      @executable_path/../Frameworks/libssl.1.1.dylib (compatibility version 1.1.0, current version 1.1.0)
      @executable_path/../Frameworks/libcrypto.1.1.dylib (compatibility version 1.1.0, current version 1.1.0)
      /usr/lib/libresolv.9.dylib (compatibility version 1.0.0, current version 1.0.0)
      /usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1311.100.3)
```

*Windows*:  
Use [Dependency Walker](https://www.dependencywalker.com/)  
Expected output will look something like this:  
```
PIA-OPENVPN.EXE  
  ...  
  LIBCRYPTO-1.1-X64.DLL  
  LIBSSL-1.1-X64.DLL  
  ...  
```
This also means that when the library is loaded (dynamically at run-time)  
every symbol must be resolved, otherwise the loading will not be successful.

### Qt

Qt is made of multiple dynamic libraries, shipped together with all the other dependencies.  
The current known Qt components that use/load Openssl libs are:
  - QtNetwork (called libQt5Network.so.5 on Linux)

Qt is loading Openssl dynamically at run-time and it is **NOT** linking against it at build-time.  
(Dynamic loading / Run-time dynamic linking)  
This makes the library more resistant to failures because not all the symbols must be resolved when the library is loaded.  

If a function is called and that function is not present in the loaded dynamic library,  
the error will be thrown only at run-time during the attempted function call.  
This is an example of an unresolved Openssl function call:  
```
[qt.network.ssl][??:0][warning] QSslSocket: cannot call unresolved function SSL_get_peer_certificate
```

(start rough part)  
Qt also implements another extra layer (pimpl pattern) to recover from missing functions errors.  
This layer uses RESOLVEFUNC.  
Due to how Windows loads libraries, this layer is not useful since trying to load a library that is missing  
symbols is going to fail.  
(end rough part)  

#### Specific details about Qt 5.X.X

Qt 5 supports up to Openssl 1.1.1.  
If it cannot find the library in the rpath, it will also look in system paths.  
On *Linux* it will look in:
1. rpath
2. LD_LIBRARY_PATH
3. The ld cache
4. System paths

On *Windows*:
1. Known DLLS (special case for special system DLLs)
2. Application Directory (C:\Program files\PIA)
3. System folders
4. PATH

Since on Windows the library is not loaded at startup, but later using `LoadLibrary`  
(which is the Windows equivalent of Linux `dlopen()`),  
the directory in which to look for the dlls can be overrided by `SetDefaultDllDirectories`.   

If it cannot find Openssl 1.1.1 anywhere, then it will load a different version (like Openssl 3.X.X).

It is unclear if building Openssl 3.X.X with this custom define  
`#define OPENSSL_API_COMPAT=10101`  
actually solves the issue of missing functions.  
While Linux and macOS can tolerate the failure, in Windows that is not possible  
due to how it handles dynamic libs.  

The only way to "cleanly" use Openssl 3.X.X with Qt, seems to be to upgrade to Qt 6. 

### pia-daemon

pia-daemon is loading Openssl dynamically at run-time and it is **NOT** linking against it at build-time.  
(Dynamic loading / Run-time dynamic linking)  
Same as Qt.  

To check, at runtime, what library is being loaded by the pia daemon, run:
```
$ sudo bpftrace ~/bpftrace/trace_dlopen.bt -c "./pia-daemon" 2>&1 | grep crypto
pia-daemon(212044) called dlopen: /opt/piavpn/lib/haswell/libcrypto.so.3
pia-daemon(212044) called dlopen: /opt/piavpn/lib/haswell/libcrypto.so.3.so
pia-daemon(212044) called dlopen: /opt/piavpn/lib/libcrypto.so.3
pia-daemon(212044) called dlopen: libcrypto.so.1.1

$ cat ~/bpftrace/trace_dlopen.bt
uprobe:/lib/x86_64-linux-gnu/libc.so.6:dlopen
{
    printf("%s(%d) called dlopen: %s\n", comm, pid, str(arg0));
}
```  

To see which shared libs are loaded at runtime:  
`sudo cat /proc/$(pgrep pia-daemon)/maps`  
or:  
`sudo lsof -p $(pgrep pia-daemon)`  
