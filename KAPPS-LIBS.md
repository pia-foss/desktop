# KApps libraries

The KApps libraries are modules used by PIA Desktop that also expose C-linkage public APIs.  These do not depend on Qt and are not brand-specific.

A simple application using the logger and some sample APIs is included in `pia_desktop/tools/kapps-lib-test`.

## Product integration

To incorporate these libraries in your build process:

* add inc/ from the library artifact to your header include directories
* add lib/ from the library artifact to your library search path (and bin/ on Windows)
* include headers like `<kapps_core/logger.h>`, `<kapps_net/firewall.h>` as needed
* link to `kapps_core`, `kapps_net`, etc.
* ship the dynamic libraries with your product

## Supportability

To troubleshoot field issues should they occur, be sure you:

* capture debug logging from the libraries using the logger API in kapps_core
* on Windows, preserve PDB symbol files needed to analyze crash dumps

### Logging

Enable debug logging and configure a debug log message callback to consume debug messages.  Capture these messages with your program's debug logging.  Note that the callback can be invoked on any thread, including APC threads on Windows, but that invocations are serialized by the logger's mutex.

Simple example from kapps-lib-test:

```
void initLogging()
{
    // Enable logging and set up a logging sink (using the C-linkage public API)
    ::DtopCoreEnableLogging(true);
    // Use our log sink as the callback.  We don't need any context in this
    // example.
    ::DtopCoreLogCallback sinkCallback{};
    sinkCallback.pWriteFn = &writeLogMsg;
    // DtopCoreLogInit copies the callback struct, so we can safely let
    // sinkCallback be destroyed
    ::DtopCoreLogInit(&sinkCallback);
}
```

See kapps-lib-test for more details, including a sample message callback, and see kapps_core/logger.h for more information on the logger APIs.

### Debug symbols

PDB symbol files are included in the library artifact on Windows.  These don't have to be shipped with the application, but should be preserved for later postmortem dump debugging should problems occur.

## Building

To build the libraries, set up the pia_desktop build environment as detailed in pia_desktop/README.md.  `rake libs` builds all libs and the output artifact; `rake tools` builds all tools including the test application.

Currently, Qt is still needed for the build process, as the MSVC toolchain selection in the build scripts selects the same version of MSVC as was used to build Qt.  This requirement could probably be relaxed with some build refactoring.
