# Private Internet Access Desktop Client

This is the desktop client for the Private Internet Access VPN service. It consists of an unprivileged thin GUI client (the "client") and a privileged background service/daemon (the "daemon"). The daemon runs a single instance on the machine and is responsible for not only network configuration but also settings and account handling, talking to PIA servers as necessary. The client meanwhile runs in each active user's desktop and consists almost entirely of presentation logic. No matter how many users are active on a machine, they control the same single VPN instance and share a single PIA account.

The project uses Qt 5 for cross-platform development, both in the client and daemon. The client GUI is based on Qt Quick, which uses declarative markup language and JavaScript and offers hardware accelerated rendering when available. Qt and Qt Quick tend to be more memory and CPU efficient compared to web-based UI frameworks like Electron or NW.js.


## Building and developing

The client is intended to be built on the target platform; Windows builds are built on Windows, macOS builds on macOS, and Linux builds on Ubuntu.

The entire product is built using [Qbs](http://doc.qt.io/qbs/) which is part of Qt. Qbs can be invoked from the command line but the recommended way to develop is to use Qt Creator, which has strong built-in support for Qbs projects.

Dependencies such as [OpenVPN](https://github.com/pia-foss/desktop-openvpn) and the [Windows TAP driver](https://github.com/pia-foss/desktop-tap) are included as precompiled binaries under the `deps` directory in this project for convenience. To recompile any of these, please refer to their corresponding directories and/or repositories for build instructions.

### Prerequisites:

- On **all platforms**:
  - [Git](https://git-scm.com/) 1.8.2 or later with [Git LFS](https://github.com/git-lfs/git-lfs/wiki/Installation) installed
  - [Qt 5.11](https://www.qt.io/download) or later (open source edition)
  - *Optional:* [Node.js](https://nodejs.org) (for auxiliary scripts)
- On **Windows**:
  - [Visual Studio Community 2017](https://www.visualstudio.com/downloads/) or [Build Tools for Visual Studio 2017](https://www.visualstudio.com/downloads/)
     - The Windows SDK must be at least 10.0.17763.0
     - Install the "Windows 8.1 SDK and UCRT SDK" to get the UCRT redistributable DLLs for 7/8/8.1
  - Debugger: Install Debugging Tools from the [Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)
     - The VS installer doesn't include the Console Debugger (CDB), which is needed to debug in Qt Creator.  More info: [Setting Up Debugger](https://doc.qt.io/qtcreator/creator-debugger-engines.html)
  - [7-zip](https://www.7-zip.org/)
- On **macOS**:
  - Sierra or newer
  - Up-to-date version of Xcode
- On **Linux**:
  - Ubuntu 16.04 or newer
  - The following development packages:
    - `build-essential`
    - `mesa-common-dev`
    - `clang` (GCC can also be used, but Clang is recommended)
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

### Building for distribution

To make completely signed and distributable builds, use the build-<platform> scripts in the scripts/ directory.  Environment variables need to be set to specify code signing parameters.

#### Windows

Set environment variables:

| Variable | Value |
|----------|-------|
| PIA_SIGNTOOL_CERTFILE | Path to certificate file (if signing with PFX archived cert) |
| PIA_SIGNTOOL_PASSWORD | Password to decrypt cert file (if signing with encrypted PFX archived cert) |
| PIA_SIGNTOOL_THUMBRPINT | Thumbprint of certificate - signs with cert from cert store instead of PFX archive |

Then call `scripts/build-windows.bat`, or `scripts/build-windows.bat <brand>` to build a brand other than PIA.

#### Mac

Set environment variables:

| Variable | Value |
|----------|-------|
| BRAND | (Optional) Brand to build (defaults to `pia`) |
| PIA_CODESIGN_CERT | Common name of the signing certificate.  Must be the complete common name, not a partial match. |
| PIA_APPLE_ID_EMAIL | Apple ID used to notarize build. |
| PIA_APPLE_ID_PASSWORD | Password to Apple ID for notarization. |
| PIA_APPLE_ID_PROVIDER | (Optional) Provider to use if Apple ID is member of multiple teams. |

Call `scripts/build-macos.sh`.

#### Linux

Set environment variables:

| Variable | Value |
|----------|-------|
| BRAND | (Optional) Brand to build (defaults to `pia`) |

Then call `scripts/build-linux.sh --configure --clean --package`.

### Project files

All main parts of the project are buildable from a master build file, `pia_desktop.qbs`, which can be opened directly in Qt Creator or built from the command line. It contains a hierarchy of subprojects:

- `client`: the GUI frontend
- `daemon`: the background service/daemon
- `extras`: auxiliary components:
  - `support-tool`: crash reporter frontend
  - `translations`: updating and embedding localized strings
  - `windows`/`macos`/`linux`: platform-specific components such as installers
- `tests`: unit test suites

To build and package the actual installer packages, use the `build-*` scripts in the `scripts` directory. These scripts have a few extra requirements for using:

- Windows:
  - Dependencies should be auto-detected if installed in standard paths on the C:\ drive
  - Specify the `PIA_SIGNTOOL_CERTFILE` and `PIA_SIGNTOOL_PASSWORD` environment variables to produce signed builds
- macOS:
  - `qbs` and `macdeployqt` need to be in your `PATH` (or specified via `QBS` and `MACDEPLOYQT`)
  - A valid Qbs default profile needs to be configured (as [described](http://doc.qt.io/qbs/configuring.html) [here](http://doc.qt.io/qbs/qt-versions.html))
  - Specify the `PIA_CODESIGN_CERT` environment variable variable to produce signed builds
    - This must be the full common name of your cert, not a partial match.
    - See "Mac installation" below to generate a self-signed cert, and remember to unlock your keychain
- Linux:
  - Qt 5.11.1 or later needs to be installed, along with packages: `build-essential`, `clang` and `curl`.
  - You need to add `qbs` and `qmake` to your `PATH`. If you have installed Qt at `$HOME/Qt5.12.0` your `$PATH` can be set as `export PATH=$HOME/Qt5.12.1/5.12.1/gcc_64/bin:$HOME/Qt5.12.1/Tools/QtCreator/bin:$PATH`. Please adjust this depending on where you have Qt installed.
  - You must run `./scripts/build-linux.sh --configure` once before using the script.

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
