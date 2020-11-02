# Private Internet Access Desktop Client

This is the desktop client for the Private Internet Access VPN service. It consists of an unprivileged thin GUI client (the "client") and a privileged background service/daemon (the "daemon"). The daemon runs a single instance on the machine and is responsible for not only network configuration but also settings and account handling, talking to PIA servers as necessary. The client meanwhile runs in each active user's desktop and consists almost entirely of presentation logic. No matter how many users are active on a machine, they control the same single VPN instance and share a single PIA account.

The project uses Qt 5 for cross-platform development, both in the client and daemon. The client GUI is based on Qt Quick, which uses declarative markup language and JavaScript and offers hardware accelerated rendering when available. Qt and Qt Quick tend to be more memory and CPU efficient compared to web-based UI frameworks like Electron or NW.js.


## Building and developing

The client is intended to be built on the target platform; Windows builds are built on Windows, macOS builds on macOS, and Linux builds on Ubuntu.

The entire product is built using rake, using the supporting framework in the `rake/` directory.

Dependencies such as [OpenVPN](https://github.com/pia-foss/desktop-openvpn) and the [Windows TAP driver](https://github.com/pia-foss/desktop-tap) are included as precompiled binaries under the `deps` directory in this project for convenience. To recompile any of these, please refer to their corresponding directories and/or repositories for build instructions.

### Prerequisites:

- On **all platforms**:
  - [Git](https://git-scm.com/) 1.8.2 or later with [Git LFS](https://github.com/git-lfs/git-lfs/wiki/Installation) installed
  - [Qt 5.15](https://www.qt.io/download) or later (open source edition)
    - Install CMake (under development tools) to use this project in Qt Creator
  - *Optional:* [Node.js](https://nodejs.org) (for auxiliary scripts)
- On **Windows**:
  - [Visual Studio Community 2019](https://www.visualstudio.com/downloads/) or [Build Tools for Visual Studio 2019](https://www.visualstudio.com/downloads/)
     - Requires VS 16.7 or later
     - The Windows SDK must be at least 10.0.17763.0
     - Install the "Windows 8.1 SDK and UCRT SDK" to get the UCRT redistributable DLLs for 7/8/8.1
  - Debugger: Install Debugging Tools from the [Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)
     - The VS installer doesn't include the Console Debugger (CDB), which is needed to debug in Qt Creator.  More info: [Setting Up Debugger](https://doc.qt.io/qtcreator/creator-debugger-engines.html)
  - [Ruby](https://rubyinstaller.org/) - includes Rake
  - [7-zip](https://www.7-zip.org/)
- On **macOS**:
  - High Sierra or newer
  - Up-to-date version of Xcode
  - Ruby, can be installed using [Homebrew](https://brew.sh) with `brew install ruby`
- On **Linux**:
  - Ubuntu 18.04 or newer
  - The following development packages:
    - `build-essential`
    - `rake`
    - `clang`
    - `mesa-common-dev`
    - `patchelf`
    - `libnl-3-dev` (PIA can run without the library installed, but the headers are needed to build)
    - `libnl-route-3-dev`
    - `libnl-genl-3-dev`

### Cloning the repository

Before cloning the Git repository, first make sure [Git LFS is installed](https://github.com/git-lfs/git-lfs/wiki/Installation) and initialized:

```console
> git lfs version
git-lfs/2.3.4 (GitHub; windows amd64; go 1.8.3; git d2f6752f)

> git lfs install
Updated git hooks.
Git LFS initialized.
```

After this, cloning the repository normally should also fetch the precompiled binaries:

```console
> git clone https://github.com/pia-foss/desktop.git
...
Filtering content: 100% (24/24), 17.13 MiB | 1.89 MiB/s, done.
```

### Build system

The following targets can be passed to `rake`.  The default target is `stage`, which stages the built client, daemon, and dependencies for local testing (but does not build installers, tests, etc.)

| Target | Explanation |
|--------|-------------|
| (default) | Builds the client and daemon; stages executables with dependencies in `out/pia_debug_x86_64/stage` for local testing. |
| `test` | Builds and runs unit tests; produces code coverage artifacts if possible on the current platform (requires clang 6+) |
| `installer` | Builds the final installer artifact, including code signing if configured. |
| `export` | Builds extra artifacts needed from CI but not part of any deployable artifact (currently translation exports) |
| `integtest` | Builds the integration test artifact (ZIP file containing deployable integration tests) |
| `artifacts` | Builds all artifacts and copies to `out/pia_debug_x86_64/artifacts` (depends on most other targets, execpt `test` when coverage measurement isn't possible) |
| `all` | All targets. |

>>>
:point_right: In 2.3.0 and earlier, PIA used a qbs-based build system, along with postprocessing to generate final installer artifacts in the various platform build scripts.  This is now all done using Rake, the qbs build system has been removed.
>>>

#### Configurations

The build system has several properties that can be configured, either in the environment or by passing the appropriate variables to `rake`.

These are implemented in `rake/build.rb`.  The output directory name includes the current brand, variant, and architecture.

| Variable | Values | Default | Explanation |
|----------|--------|---------|-------------|
| `VARIANT` | `debug`, `release` | `debug` | Create a debug build (unoptimized, some compression levels reduced for speed), or release build (optimized, maximum compression). |
| `ARCHITECTURE` | `x86_64`, `x86` | `x86_64` | Select an alternate architecture.  Windows supports `x86` as well as `x86_64`, Mac and Linux only support `x86_64`. |
| `BRAND` | (directories in `brands/`) | `pia` | Build an alternate brand. |

#### Variables

Some additional environment variables can be configured:

| Variable | Example | Explanation |
|----------|---------|-------------|
| `QTROOT` | /opt/Qt/5.12.8 | Path to the installed Qt version, if qt.rb can't find it or you want to force a specific version |

### Qt Creator

To open the project in Qt Creator, open CMakeLists.txt as a project.  This CMake script defines targets for Qt Creator and hooks them up to the appropriate rake tasks, which allows Qt Creator to build, run, and debug targets.

Some specific configuration changes are useful in Qt Creator:

#### File Locator

The file locator (Ctrl+K / Cmd+K) can only locate files referenced by targets by default.  The CMake script allows it to find most C++ source, but it can't find QML source or other ancillary files.  Enable the "Files in All Project Directories" filter by default:

1. Open Qt Creator's Preferences
2. Go to Environment > Locator
3. Next to "Files in All Project Directories", check the box for "Default"
4. Select "Files in All Project Directoreis" and click "Edit..."
5. Add the exclusion patterns "*/out/*" and "*/node_modules*" (to exclude build outputs and modules installed for ancillary JS utility scripts)

This is a good idea anyway because it also allows you to find files not used by a target, like Rakefile, .gitignore, build scripts, etc.

#### Default Target

Qt Creator's default target is 'all', which is hooked up to rake's default - the staged installation only.  You can also set Qt Creator's default to really build everything (including running tests) or to build the current executable (useful to build specific tests, but not needed to debug them from Qt Creator).

1. Go to the Projects page
2. Select "Build" under current kit"
3. Under "Build Steps", expand the CMake build step
4. Select a different target instead of "all":
   - "rake-all" will build everything, including running unit tests
   - "Current executable" will build the current target you have selected in
     Qt Creator - for client/cli/daemon this builds the staging area, but for
     integtest or unit tests it will build the test instead

#### Kit and Qt version

Qt Creator will still ask to select a kit, which includes a Qt version, compiler, etc.  This has no effect on the build output - the Rake scripts find Qt and the compiler on their own, which allows them to be run with no prior setup.  Select the same Qt version and compiler in Qt Creator that the rake script uses, so Qt Creator's code model will work.

On Windows, Qt Creator will run rake in an environment set up using vcvarsall.bat, which is not necessary (since the rake scripts run vcvarsall.bat automatically) but may cause stale MSVC variables to remain in the build environment if you update MSVC.  Just do a clean and re-run cmake from Qt Creator, and the environment should be reset.

### Building for distribution

`rake artifacts` (or `rake all`) produces the final artifacts for distribution, including signing if code signing details are provided.  Code signing environment variables are defined below.

Build scripts in the `scripts` directory are also provided that clean, then build for all architectures supported on a given platform.

#### Windows

Set environment variables:

| Variable | Value |
|----------|-------|
| BRAND | (Optional) Brand to build (defaults to `pia`) |
| PIA_SIGNTOOL_CERTFILE | Path to certificate file (if signing with PFX archived cert) |
| PIA_SIGNTOOL_PASSWORD | Password to decrypt cert file (if signing with encrypted PFX archived cert) |
| PIA_SIGNTOOL_THUMBRPINT | Thumbprint of certificate - signs with cert from cert store instead of PFX archive |

Then call `scripts/build-windows.bat`

#### Mac

Set environment variables:

| Variable | Value |
|----------|-------|
| BRAND | (Optional) Brand to build (defaults to `pia`) |
| PIA_CODESIGN_CERT | Common name of the signing certificate.  Must be the complete common name, not a partial match. |
| PIA_APPLE_ID_EMAIL | Apple ID used to notarize build. |
| PIA_APPLE_ID_PASSWORD | Password to Apple ID for notarization. |
| PIA_APPLE_ID_PROVIDER | (Optional) Provider to use if Apple ID is member of multiple teams. |

Call `scripts/build-macos.sh`

A certificate is required for the Mac build to be installable (even for local builds), see below to generate a self-signed certificate for local use.  Unsigned builds can be manually installed by running the install script with `sudo`.

#### Linux

Set environment variables:

| Variable | Value |
|----------|-------|
| BRAND | (Optional) Brand to build (defaults to `pia`) |

Then call `scripts/build-linux.sh`.

### Running and debugging

Each platform requires additional installation steps in order for the client to be usable (e.g. the Windows TAP adapter needs to be installed). The easiest way to perform these steps is to build and run an installer, after which you can stop and run individual executables in a debugger instead.

To debug your own daemon, the installed daemon must first be stopped:

- **Windows**: Run `services.msc` and stop the Private Internet Access Service
- **macOS**: Run `sudo launchctl unload /Library/LaunchDaemons/com.privateinternetaccess.vpn.plist`
- **Linux**: Run `sudo systemctl stop piavpn`

The daemon must run as root. Consult your IDE/debugger documentation for how to safely run the debugger target as root.

### Mac installation

Installation on Mac uses `SMJobBless` to install a privileged helper that does the installation.

Mac OS requires the app to be signed in order to install and use the helper.  If the app is not signed, it will not be able to install or uninstall itself (you can still install or uninstall manually by running the install script with `sudo`.)

`PIA_CODESIGN_CERT` must be set to the full common name of the certificate to sign with.  (`codesign` allows a partial match, but the full CN is needed for various Info.plist files.)

To test installation, you can generate a self-signed certificate and sign with that.

* Open Keychain Access
* Create a new keychain
   1. Right-click in the Keychains pane and select "New Keychain..."
   2. Give it a name (such as "PIA codesign") and password
* Generate a self-signed code signing certificate
   1. Select your new keychain
   2. In the menu bar, select Keychain Access > Certificate Assistant > Create a Certificate...
   3. Enter a name, such as "PIA codesign", it does not have to match the keychain name
   4. Keep the default identity type "Self Signed Root"
   5. Change the certificate type to "Code Signing"
   6. Click Create, then Continue
* Optional - disable "lock after inactivity" for your keychain
   1. Right-click on the keychain and select "Change Settings for Keychain"
   2. Turn off locking options as desired
* Use the certificate to sign your PIA build
   * Qt Creator:
      1. Select Projects on the left sidebar, and go to the build settings for your current kit
      2. Expand "Build Environment"
      3. Add a new variable named `PIA_CODESIGN_CERT`, and set the value to the common name for your certificate
   * Manual builds with `build-macos.sh`
      1. Just set `PIA_CODESIGN_CERT` to the common name you gave the certificate when building

## Contributing

By contributing to this project you are agreeing to the terms stated in the [Contributor License Agreement](CLA.md). For more details please see our [Contribution Guidelines](https://pia-foss.github.io/contribute) or [CONTRIBUTING](/CONTRIBUTING.md).

## Licensing

Unless otherwise noted, original source code is licensed under the GPLv3. See [LICENSE](/LICENSE.txt) for more information.
