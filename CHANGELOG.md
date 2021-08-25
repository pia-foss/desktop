# Changelog

### v3.0.0
* Redesigned the Settings window to improve categorization and allow for future growth
* Separated the three “Kill Switch” setting choices into two separate settings - “VPN Kill Switch” and “Advanced Kill Switch”
* Automation rule triggers on the dashboard are now removed if the rule is deleted
* The “Usage” tile now adjusts units based on the amount of data transferred
* Fixed navigation order issues with VoiceOver on macOS in some Settings pages
* Fixed navigation order of overlay dialogs in Settings window for Windows screen readers
* Fixed an issue causing overlay tips to stop working after removing an Automation rule

### v2.10.0
* Reduced memory and CPU usage of the graphical client
* Updated icons and graphics
* Connection stats can be sent to PIA on an opt-in basis to help improve our service
* Added a CLI get/set type for the Allow LAN setting
* The split tunnel UI on Windows now displays executable paths instead of link paths for Start Menu apps
* WireGuard now works correctly on macOS and Linux when jumbo frames are enabled on the network interface
* The PIA daemon on Linux no longer writes to stderr when run as a service to avoid flooding system logs
* In-app updates on Linux now detect xfce4-terminal on systems without an x-terminal-emulator symlink
* Fixed a crash on Windows caused by some older OpenGL drivers
* Fixed an install issue on Linux that prevented creation of the piavpn group in some cases
* Fixed an issue causing the support tool to appear more than once on Linux in some cases
* Fixed libxcb dependencies in Linux arm64 build

### v2.9.0
* Updated to OpenVPN 2.5.1 and OpenSSL 1.1.1k
* Improved accessibility of the "Add Automation Rule" dialog
* Split tunnel on Linux now applies the Split Tunnel Name Servers setting to DNS requests routed through the host (most containers / VMs)
* "Submit Debug Logs" now shows an indicator while collecting diagnostics
* Added additional split tunnel diagnostics for macOS
* Linux installations no longer require ifconfig
* ip no longer has to be at /sbin/ip on Linux
* The PIA daemon on Linux OpenRC systems no longer waits for a network connection before starting
* Fixed restarting the PIA daemon after an upgrade on Linux SysVinit systems

### v2.8.1
* Fixed an issue in macOS split tunnel that prevented the VPN from connecting when the killswitch was set to Always

### v2.8.0
* Automation rules can now be created in Settings to automatically connect or disconnect when joining networks
* Fixed macOS split tunnel issues preventing access to LAN devices or bypassed subnets on some systems
* Fixed issues preventing LAN DNS servers from working in the Custom DNS setting
* Fixed the Cmd+W shortcut in the Changelog window on macOS

### v2.7.1
* Fixed an issue causing WireGuard connections to fail on some systems running macOS 10.13
* Fixed an issue causing PIA to stop responding on some systems running macOS 11
* Additional diagnostics on Windows

### v2.7.0
* Split tunnel on macOS no longer uses a network kernel extension
* Split tunnel now supports macOS 11.0 (Big Sur)
* Service notifications can now be shown below the Connect button
* Added support for Linux ARM build configurations (armhf and arm64)
* Linux builds are now made on Debian Stretch
* Ubuntu 16.04 is no longer supported (libstdc++ 6.0.22 is now required)
* Added support for renewing Dedicated IPs
* "Bypass" apps on Linux now also bypass the PIA killswitch
* OpenVPN now always uses RSA-4096 for the server authentication handshake
* OpenVPN CBC ciphers now always use SHA-256 for data authentication
* Removed the Data Encrytion "None" setting for OpenVPN
* Updated to Qt 5.15.2, OpenVPN 2.4.10, OpenSSL 1.1.1i
* Fixed an issue causing bypass apps on Linux to occasionally use VPN DNS
* Fixed an issue on Linux causing OpenVPN to fail to connect when PATH exceeds 256 characters
* Fixed an issue preventing the Built-in Resolver from working reliably on some Windows systems

### v2.6.1
* Fixed a crash when connecting to a region with no servers available for the current protocol

### v2.6.0
* Removed support for the legacy PIA network
* Added support for upcoming Dedicated IP feature
* Temporarily unavailable regions are displayed in the regions list and ignored by automatic selection
* Regions now report multiple servers per region for improved connection resiliency
* Minor improvements to regions list UI and accessibility
* Improved word breaking in Thai translation
* Fixed some Shadowsocks servers not appearing with next-gen network
* Fixed an issue preventing OpenVPN from connecting on Fedora 33
* Fixed an issue occasionally allowing domains that should be blocked by MACE to remain cached on the system
* Fixed accessibility focus indications for drop-down buttons
* Fixed launching client after install, and launching downloaded updates on some Linux environments
* Fixed missing accessibility annotations on Shadowsocks proxy region list

### v2.5.1
* Fixed a crash that occurred when geo-located regions were disabled in Settings
* Fixed an issue preventing the crash reporter from starting for client crashes

### v2.5.0
* Split tunnel on Windows now also splits DNS traffic
* Added "Name Servers" setting to Split Tunnel on Windows and Linux
* Region locations and translations are now updated automatically
* Split tunnel is disabled on macOS 11.0 due to removal of network kernel extensions
* Added `pubip` type to `piactl get/monitor` (thanks Chase Wright!)
* Fixed executable signing on Windows

### v2.4.0
* Windows hardware acceleration now uses Direct3D 11 instead of OpenGL
* PIA on Windows now requires Windows 8 or later
* Split tunnel app rules on Linux now also split DNS traffic
* Routed packets on Linux are now protected by the PIA killswitch (includes most containers and VMs)
* Split tunnel can now bypass routed packets on Linux
* Fixed a crash on macOS caused by changing screen layouts
* Fixed an issue causing installation to hang in some cases on macOS

### v2.3.2
* Added notification for OS versions that are no longer supported
* Removed network setting from Help page

### v2.3.1
* Fixed a possible daemon crash on macOS when split tunnel was enabled
* Fixed dependency issues on some Linux distributions
* Detect additional graphics drivers for automatic safe graphics mode on Windows

### v2.3
* Next Generation network is now the default
* Added the Connection tile
* Updated Qt to 5.15.0
* PIA on macOS now requires 10.13
* Fixed several issues relating to installation or uninstallation on Windows in Safe Mode
* Fixed an issue causing a memory leak on some Windows systems when Windows suspends pia-client to save power
* Fixed an issue preventing split tunnel from working with WireGuard on some newer Linux distributions
* Fixed DNS routing issues with split tunnel on Linux systems not using systemd-resolved
* Fixed an issue causing the WireGuard userspace method to occasionally fail to connect on some Linux systems
* Removed unneeded WireGuard kernel module logging on Linux

### v2.2.2
* Fixed an issue causing high CPU usage on some Linux systems
* Fixed an issue causing WireGuard to disconnect in some cases using split tunnel with All Other Apps set to Bypass

### v2.2.1
* Fixed an issue on Windows causing the PIA service to crash when connecting with some Split Tunnel configurations

### v2.2
* Split tunnel is now supported with WireGuard
* Geo-located regions are supported
* The {{BRAND_SHORT}} Next Generation network is now in preview
* Added Built-in Resolver option to Name Servers setting
* Removed Handshake testnet resolver from Name Servers setting (testnet no longer exists, hnsd does not support mainnet)
* Added 'requestportforward' option to '{{BRAND_CODE}}ctl get/set' to control port forwarding setting
* Improved DNS leak protection compatibility with macOS 10.15.4 and later
* Improved compatibility of split tunnel on Windows with other WFP callout drivers
* Fixed an issue preventing IP split tunnel rules from being disabled on Windows in some cases
* Fixed layout of the killswitch warning in some translations
* Changed title of windowed dashboard to "{{BRAND}}"
* The regions list keeps its scroll position as latencies are updated
* Diagnostic improvements in debug reports

### v2.1
* Split tunnel "bypass" rules can be created for IP addresses and subnets
* WireGuard now supports network roaming on Mac in addition to Windows and Linux
* New {{BRAND_CODE}}ctl commands to log in and log out - '{{BRAND_CODE}}ctl login', '{{BRAND_CODE}}ctl logout'
* New {{BRAND_CODE}}ctl command to enable killswitch and/or VPN connections without using the graphical client - '{{BRAND_CODE}}ctl background'
* Fixed an issue that prevented WireGuard from connecting on Windows if the computer had been shut off unexpectedly
* Fixed an issue on some Linux resolvconf systems that could block DNS incorrectly when using WireGuard

### v2.0.2
* Improved handling of several Mac applications with split tunnel, such as Mail and Calendar
* Split tunnel on Linux attempts to mount net_cls automatically if it's not mounted (as on Fedora)
* The WireGuard connectivity timeout is customizable
* Improved support for IPv6 networks when the Allow LAN setting is enabled
* Fixed an issue that prevented Use Existing DNS from working on some Mac systems
* Fixed an issue causing WireGuard to take a long time to connect on some Windows systems
* Updated Qt to 5.12.8
* Updated OpenVPN to 2.4.9
* Updated OpenSSL to 1.1.1g
* Security improvements

### v2.0.1
* Fixed long DNS resolution times on Windows on some systems

### v2.0
* WireGuard is now supported as a connection method
* WireGuard on Windows requires Windows 8 or later
* Some settings are not yet supported with WireGuard - split tunnel, port forwarding, and proxy
* Fixed the Mail app on Windows 10 with split tunnel (shares an app family with Calendar)
* Fixed terminal emulator support for Terminator and other terminals on Linux
* VPN IP and forwarded port appear more quickly than in 1.8
* Improved reliability of support tool submissions
* Security improvements

### v1.8
* Split tunnel apps can be configured to use the VPN only or to bypass the VPN
* The default behavior can be set to "Bypass VPN" to use the VPN only for specific apps
* Fixed TCP localhost connections for split tunnel apps on Windows
* Fixed split tunnel for Mac apps that bind to specific ports
* Fixed focus behavior on Mac when closing windows with the keyboard
* Fixed LAN routing problems when split tunnel is enabled on Linux
* Improved reliability of the port forwarding feature
* Updated Qt to 5.12.6
* Updated OpenVPN to 2.4.8
* Updated OpenSSL to 1.1.1d
* Updated TAP adapter to 9.24.2 on Windows

### v1.7
* The Shadowsocks proxy setting can be used to redirect the VPN connection through a Shadowsocks region
* Added the '{{BRAND_CODE}}ctl monitor' command
* Added the 'connectionstate' type to '{{BRAND_CODE}}ctl get'
* Improved firewall rules on Linux to mitigate CVE-2019-14899 on affected distributions
* Improved handling of crashes caused by graphics drivers on Windows
* Fixed an issue preventing apps from being selected for App Exclusions on macOS 10.15
* Fixed an issue causing Windows 10 1507 / LTSB 2015 to restart on shutdown

### v1.6.1
* Security improvements in the Mac OS installer

### v1.6
* VPN Snooze allows temporarily disconnecting the VPN connection.
* Added "{{BRAND_CODE}}ctl" - a command-line interface to control the client.
* Connection loss is detected more quickly.
* Fixed issues in the App Exclusions feature that could occur when switching network connections.
* App Exclusions supports macOS 10.12.
* Fixed detection of the iptables version for some Linux distributions.
* App Exclusions is improved for listening sockets on Linux.
* The dashboard repositions correctly if the screen resolution changes on macOS.
* Fixed an issue that could cause long delays when the client starts on login.
* Security improvements.

### v1.5.1
* Added a "Help" link to the App Exclusions feature in Settings

### v1.5
* Split tunneling allows applications to bypass the VPN using the App Exclusions feature.
* Excluded applications bypass the VPN and connect directly to the Internet.
* Windows: This feature currently requires Windows 7 SP1.  Support for Windows Store apps requires Windows 10.
* Mac: This feature currently requires macOS 10.13.
* Linux: This feature currently requires iptables 1.6.1 with systemd network control groups on Linux.

### v1.4
* Support connecting via a SOCKS5 proxy
* Notarize application on Mac for compatibility with 10.15
* Update Mac installer to improve compatibility with 10.15
* Minor translation fix for French
* Minor firewall rule fix on Windows

### v1.3.3
* Support both DHCP-based configuration (like 1.2.1) and static configuration (like 1.3.1) on Windows
* Update Handshake to fix linkage on some Linux distributions and with an additional seed

### v1.3.2
* Use DHCP-based configuration of the TAP adapter on Windows

### v1.3.1
* Fixed issues on Windows when the TAP adapter name contained non-ASCII characters
* Fixed minor translation issues

### v1.3
* Countries can be marked as favorite regions
* "Auto" region selects a port forwarding region when port forwarding is enabled
* Support Handshake name resolution (using Handshake's testnet)
* Support some Linux distributions using sysvinit
* Persist the sort selection on the regions page
* Improve robustness of TAP adapter configuration on Windows
* Try alternate protocols and ports automatically if the chosen settings cannot connect

### v1.2.1
* Fixed an issue causing the VPN to stay connected when logging out of the OS.

### v1.2
* Tiles can be rearranged with drag-and-drop
* Added a setting for "windowed" or "attached" dashboard on all platforms
* Preserve killswitch and VPN connection if client exits unexpectedly
* Fix reconnecting after suspend on Windows
* Fix multiple crashes, in particular crashes after suspend on Windows
* Improve software rendering backend
* Improved accessibility of Changelog window
* Minor fixes for right-to-left desktops on Linux
* Update to OpenVPN 2.4.7
* Update TAP adapter on Windows to 9.23.3.601
* Added additional firewall diagnostics on Windows

### v1.1.1
* Fix occasional crashes in Windows installer
* Fix macOS installer error on certain systems

### v1.1
* Added tray icon theme setting with alternate styles in response to user feedback
* Improve reliability of VPN IP address
* Attempt to rotate through server IPs more frequently between connection attempts
* Show a warning on Windows when the TAP adapter is not installed
* Improve robustness of firewall rules on Mac OS
* Improve single-instance handling on Linux
* Improve reliability of tray icon on Linux when launched on login
* Clarify warning shown when account can't be verified
* Fix Linux HiDPI support when launched on login for some distributions
* Fix Windows installer on Windows 7 without specific Windows updates
* Fix Allow LAN setting being disabled by default after upgrading from legacy client
* Fix installation issue on Linux due to incorrect umask
* Improve appearance of pop-up tips for languages other than English

### v1.0.2
* Added option to disable accelerated graphics to fix stability issues
* Set correct group id when re-starting after a crash on Linux

### v1.0.1
* Added screen reader support
* Avoid assuming IPv6 is present
* Bring the app to the front if relaunched while running
* Avoid insecure directories on Windows
* Made tray icon more robust on Windows
* Fixed window title on Windows installer
* Fixed rare crash when enabling debug logging

### v1.0
* Added Quick Tour displayed on first run
* Slightly more robust uninstaller on Linux
* Fixed iptables handling when DNS is unavailable on Linux
* Remove legacy .desktop file when upgrading on Linux
