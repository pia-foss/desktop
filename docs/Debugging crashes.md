# All we need to load Dump Files

Whenever the daemon or client crash, we generate a `.dmp` crash file. Users can voluntarily upload these crash files when the crash occurs, and we can obtain them from our in-house CSI tool.

To check a dump locally you will need:

* minidump_stackwalk: you can build it from source from https://chromium.googlesource.com/breakpad/breakpad/ . Do install google's `depot_tools` to make your life easier.
* the right symbols for the version: we build them on CI and we include them in our candidate releases, so as long as you know the version that created the dump file you can retrieve the symbols.

Invoke it like this:

```
minidump_stackwalk crash_dump.dmp syms
```

That's it!


# A note on syms structure

`syms` is the path to a directory with a structure like this
```
syms
├── kapps_core.dylib
│   └── F1A52694F55D3222A06BA471B0603A980
│       └── kapps_core.dylib.sym
├── kapps_net.dylib
│   └── 75E4A2729B893A81A7EBA946CAFA23020
│       └── kapps_net.dylib.sym
├── pia-commonlib.dylib
│   └── B95010ADCC7837F2BADACBFC67FE935C0
│       └── pia-commonlib.dylib.sym
├── pia-daemon
│   └── 55D5C68EBC0036C3A0E8EF279CBB5CE70
│       └── pia-daemon.sym
...
```

`minidump_stackwalk` requires the symbols path to be structured in that way. 
We cannot specify individual symbol files one by one, instead we are required to adhere to this structure.
```
syms_directory
├── library_or_executable
│   └── MODULE_UUID
│       └── library_or_executable.sym
```

`MODULE_UUID` is part of the symbol file, and we can find it in the first line, e.g. this is the first line of the sym file `kapps_core.dylib.sym` from above:
```
MODULE mac arm64 F1A52694F55D3222A06BA471B0603A980 kapps_core.dylib
```
