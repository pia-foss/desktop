// PIA uses the embeddable-wg-library for WireGuard support.  Since wireguard.c
// is used on Linux, this is built and linked as a static library.  However,
// this means there would be no objects on Windows or macOS, since only the
// header is used on those platforms.
//
// Header-only libraries are usually described with a Component in the build
// system, but rather than switching between Component and Executable, it's
// easier just to add a source file so that it's always a static library with
// at least one object - even though this file doesn't actually define
// anything.
