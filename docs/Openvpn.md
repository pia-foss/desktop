# Openvpn

## General notes

Openvpn version 2.5.X supports up to Openssl 1.1.1  
Openvpn version 2.6.X introduced support for Openssl 3.X.X

Openvpn is built as a separate binary file,  
pia-daemon is responsible for starting the process with all the required arguments.  
Openvpn process stdout and stderr are redirected to pia-daemon and are logged in the same  
file that contains pia-daemon logs, called *daemon.log*.  
Refer to the README on how to enable debug logs on your system.  
A log line coming from Openvpn will likely resemble this:  
`[2023-09-07 17:02:10.895][1994][daemon.openvpnmethod][src/openvpnmethod.cpp:680][debug] "2023-09-07 19:02:10 us=889209 OpenVPN 2.5.4 x86_64-w64-mingw32 [SSL (OpenSSL)] [COMP_STUB] [AEAD] built on Dec 20 2021"`

## Regarding patches

We currently ship Openvpn with a couple of changes on top  
(check out pia_desktop_dep_build repository for more details).

The only patch that is really needed, is the one regarding driver IDs on Windows.  
The reason for this is that other vpn software could install TAP and WinTun drivers,  
so we brand the one that we install with PIA, so that only PIA openvpn process can use them,  
in order to avoid possible conflict or other processes acting on or using our network virtual adapters.
