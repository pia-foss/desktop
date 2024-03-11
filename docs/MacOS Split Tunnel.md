# MacOS Split Tunnel Extension Integration

## How to update the dependency from the pia_mac_split_tunnel repo
The pia_mac_split_tunnel github repo (https://github.com/xvpn/pia_mac_split_tunnel) builds the extension and makes it available
as a zip file artifact under 'actions', e.g https://github.com/xvpn/pia_mac_split_tunnel/actions

(1) Just click the 'workflow run' you're interested in and scroll to the bottom to find the generated zip file
  - it should be called "splitTunnel.zip", download it.
(2) After downloading this file, you will need to unzip it as it's actually a nested zip file, and we want the inner zip.
  - this inner zip should be called "stmanager.zip". Don't unzip it using finder, do it in the terminal using the `unzip` command
  otherwise finder will unzip all nested zips too, which we do not want.
(3) After extracting the inner zip ("stmanager.zip") you need to copy it into the "deps/built/mac" folder of
  your local checkout of pia_desktop.
(4) This zip file will be used by the PIA build process to properly bundle the split tunnel as part of the PIA Desktop application.
(5) Add and commit the updated "pia_desktop/deps/built/mac/stmanager.zip" file.
(6) Push to the remote pia_desktop repo.
(7) Done!

We are using a system extension by way of a CLI sub application.
This is [a way that apple does not love](https://developer.apple.com/forums/thread/744716).

Regarding building and signing PIA, nothing major changes.
We only need to be careful to sign PIA after embedding `PIA Split Tunnel.app`, but to **not** re-sign `PIA Split Tunnel.app`, otherwise the signatures break.

# Bundle changes

We settled for minimal changes to the application structure.
Simply adding `PIA Split Tunnel.app` to the `MacOS` directory in the bundle.
The system extension is owned by `PIA Split Tunnel.app`, which we can just depend on for dealing with the System Extensions.

```
Contents
    - MacOS
       - Private Internet Access
       - pia-daemon
       - ...
    - CodeSignature
    - PkgInfo
    - Info.plist
```

```
Contents
    - Frameworks
    - MacOS
       - Private Internet Access
       - pia-daemon
       - PIA Split Tunnel.app
            - Contents
                - MacOS
                    - PIA Split Tunnel
                - Library
                    - SystemExtensions
                        - com.privateinternetaccess.vpn.splittunnel.systemextension
                - ...
       - ...
    - CodeSignature
    - PkgInfo
    - Info.plist
```
