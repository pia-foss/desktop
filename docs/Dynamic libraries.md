# Handling Dynamic libraries

## General advices

### Linux: 

- Remember to build dependencies and client on the same OS version.  
- Right now we are currently using Debian 10, for both the client and the deps.  
- Using a too new OS version could result in `GLIBC_X.XX` errors in older systems.  
An example:  
```
$ ldd /pia/deps/built/linux/x86_64/libcrypto.so.3
/pia/deps/built/linux/x86_64/libcrypto.so.3: /lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.33' not found (required by /pia/deps/built/linux/x86_64/libcrypto.so.3)
/pia/deps/built/linux/x86_64/libcrypto.so.3: /lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.34' not found (required by /pia/deps/built/linux/x86_64/libcrypto.so.3)
        linux-vdso.so.1 (0x00007ffe13b3f000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f3102840000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f3103095000)
```
To check your system GLIBC version run this command: (this is the output for Debian 10)  
```
$ ldd --version
ldd (Debian GLIBC 2.28-10+deb10u2) 2.28
```
- Double check that all artifacts have 755 permission.

## Where does the system look for dynamic libraries?

On **Linux**:  
1. rpath
2. LD_LIBRARY_PATH
3. The ld cache
4. System paths

## Differences between platforms

### Linux

`dlopen()`: function used to load a dynamic library.  
`dlsym()`: used to retrieve symbols (functions or variables) from those loaded libraries.  

When loading a shared library using `dlopen()`, the system doesn't necessarily load the entire content of the library into memory.  
Instead, it just prepares to make calls into that library.  

The behaviour can change depending on which of these two flags is set:  
- `RTLD_LAZY`:  
  The first time a particular function (or variable) from the loaded library is referenced using `dlsym()`, the address of that function is looked up (or "resolved").  
  Subsequent calls to the same function don't need to perform the lookup again.  
  If a function or variable is never referenced, its address is never resolved.

- `RTLD_NOW`: 
	`dlopen()` tries to resolve all global function and variable addresses in the library immediately.  
	It's not about physically loading function code or data but about determining where everything is.  
	If a particular symbol can't be resolved (maybe it references another library that's missing or has an unresolved dependency), `dlopen()` will fail.  
	Even if your program might *never actually call* that function.  

### Windows

Delay loading in Windows means is that, even a DLL that is linked at build-time, won’t get loaded when a program starts.  
It’ll be dynamically loaded lazily on first use (i.e first symbol use).  
This is mainly just a security measure, so that safe commands like `SetDefaultDllDirectories` have a chance to kick-in.

Windows behaviour is similar to Linux `RTLD_NOW`.  
