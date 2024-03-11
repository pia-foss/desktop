Original document written by [John Mair](https://github.com/banister).

# IPv6 and Desktop

Currently PIA only supports IPv4. Neither our server infrastructure or our desktop client allow outbound IPv6 traffic while connected to the VPN. The desktop client does attempt to support some IPv6 traffic (within a LAN) but even this support is incomplete.

IPv6 has been around for decades yet only has an [adoption rate of around 30%](https://www.google.com/intl/en/ipv6/statistics.html). Nonetheless IPv6 is likely the future of the internet and we need to support it to stay ahead of the curve.

This document has two parts: 

- [The first part](#ipv6-overview) is an overview of IPv6.
- [The second part](#desktop-ipv6-integration) investigates how we'd support IPv6 in Desktop
    - We only focus on client changes, server/operations changes are out of scope.

## IPv6 Overview

Introduced in 1998 the IPv6 protocol's defining feature is an increased address space compared to IPv4. Address exhaustion (due to IPv4's relatively small 32-bit address size - around 4.3 billion addresses) has been a major concern. In contast, IPv6 addresses are 128 bits in length - providing a practically inexhaustible maximum of 340 undecillion, or 340 billion billion billion billion addresses.

One explanation for IPv6's slow adoption is that it was not designed to be backwards compatible with IPv4 - they exist on parallel networks. Hosts that support both protocols are known as dual-stack; and both stacks must be configured and secured independently with separate firewalls and routing tables.

IPv6 may appear to have some IPv4 support via IPv4-mapped addresses and the IPv6 "any" address for listening sockets; but these [connections are just delegated to the IPv4 stack](https://www.ibm.com/support/knowledgecenter/ssw_ibm_i_72/rzab6/xacceptboth.htm). A socket that accepts both IPv4 and IPv6 connections (with the appropriate stack delegatoin performed behind the scenes) is called a [dual-stack socket](http://mars.tekkom.dk/w/index.php/IPv4-Mapped_IPv6_Address).

Another defining feature of IPv6  is that addresses are globally routable and uniquely identifiable. An IPv6 enabled host uses the same IP address whether on LAN or the wider internet. Whether they are reachable or not depends on the firewall or filtering configuration, and not (as with IPv4) the presence of NAT.

**Sections**:
* [Address syntax](#address-syntax)
* [Address structure](#address-structure)
* [Multiple addresses and address types](#multiple-addresses-and-address-types)
* [Globally routable addresses](#globally-routable-addresses)
* [Link-local addresses](#link-local-addresses)
* [Unique local addresses](#unique-local-addresses)
* [Multicast addresses](#multicast-addresses)
* [Neighbor discovery](#neighbor-discovery)
* [Address assignment](#address-assignment)

### Address syntax

An IPv6 address is 128 bits in length and consists of eight 16-bit fields, with each field separated by a colon. Each field contains a hex number:

`2a03:b0c0:0002:00d0:000:000:0026:c001`

This form is often unwieldy and so the standard [allows us to abbreviate](https://en.wikipedia.org/wiki/IPv6_address#Representation) the above address to:

`2a03:b0c0:2:d0::26:c001`

By removing leading `0` s, and using `::` to represent contiguous fields of `0`. 

The `::` is used to represent however many blocks of `0000` needed to create an address of the correct length.

Another example is the IPv6 loopback address which is represented as `::1`, and is equivalent to  `127` `0`s followed by a `1`

### Address structure

Generally speaking an IPv6 address is divided into 2 parts, the first 64 bits are known as the ["Network prefix"](https://docs.oracle.com/cd/E19253-01/816-4554/ipv6-overview-170/index.html) with the last 64 bits being the ["Interface identifier"](https://docs.oracle.com/cd/E19253-01/816-4554/gbvsf/index.html) (also called the interface token). 

As an example the IPv6 address `2001:1c00::6` has a network prefix of `2001:1c00:0000:0000` (i.e the first 64 bits) and an interface identifier of `0000:0000:0000:0006` (the last 64 bits)

### Multiple addresses and address types

Unlike IPv4 it is normal (and necessary) for an IPv6 interface to have multiple addresses. At the very least an IPv6 interface will have a "globally routable" address and a "link-local" address. Some  may also have a "unique local" address. These are all examples of different IPv6 address types, of which there are many. 

In addition to the above, it is also common for an interface to have **multiple** globally routable addresses. This is normally for privacy reasons and will be explained later. IPv6 has built-in logic for determining [which address to use](https://etherealmind.com/ipv6-which-address-multiple-ipv6-address-default-address-selection/).

### Globally routable addresses

Globally routable addresses are the most important type of IPv6 addresses. They uniquely identify and expose a host to the wider internet. 

Like every IPv6 address, the global  address is divided into 2 parts - the first part (the network prefix) is provided by a service provider (ISP).

However most ISPs provide slightly less than the full 64 bit prefix leaving a small portion of bits (typically `8` or `16`) for the administrator to create [IPv6 "subnets"](https://docs.netgate.com/pfsense/en/latest/book/network/ipv6-subnets.html).

This ISP prefix together with the subnet id should make up 64 bits. As mentioned earlier, the last 64 bits of the IPv6 address are the "interface identifier". These bits are normally generated by the IPv6 host itself during autoconfiguration. 

> Hosts that share the same network prefix are usually "on link", and are directly connected to eachother via a LAN. A router is not required for them to communicate.

This global IP address is more or less equivalent to an IPv4 public ip, and identifies that host whether on LAN or on the internet.

Example:
Given the globally routable ip `2a03:b0c0:2:d0::26:c001` the "Network prefix" is the first 64 bits, which is `2a03:b0c0:2:d0` The "interface identifier" is `::26:c001`which in unabbreviated form corresponds to `0000:0000:0026:c001`

Note that all the hosts that are directly accessible (i.e on the same LAN) will share this network prefix. So you should see the `2a03:b0c0:2:d0::/16` indicated as "on link" when you view routing table information: (a "Next Hop" of `::` indicates the subnet is "on link", i.e no hop is required)

    $ route -6n
    Kernel IPv6 routing table
    Destination                    Next Hop                   Flag Met Ref Use If
    2a03:b0c0:2:d0::/64            ::                         U    256 2     4 eth0

**Multiple addresses**

As mentioned earlier, an IPv6 interface may have multiple global addresses. In addition to its stable address it may also have various "temporary" (aka privacy) addresses with a randomized interface identifier. The temporary addresses are cycled regularly to maintain anonymity for (usually) outbound connections [https://labs.ripe.net/Members/johanna_ullrich/ipv6-addresses-security-and-privacy](https://labs.ripe.net/Members/johanna_ullrich/ipv6-addresses-security-and-privacy)

In the command output below you can see there are 2 global IPv6 ips assigned to `eth0`, one marked "temporary", the other one marked "mngtmpaddr". The mngtmpaddr is typically the stable IP and is also used as a base for generating the temporary Ips.

    $ ip addr show eth0
    3: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
        link/ether 94:e6:f7:aa:25:30 brd ff:ff:ff:ff:ff:ff
        inet 192.168.1.204/24 brd 192.168.1.255 scope global dynamic noprefixroute eth0
           valid_lft 80933sec preferred_lft 80933sec
        inet6 2001:1c00:e08:faf0:bc92:c044:6d5f:5cc/64 scope global temporary dynamic 
           valid_lft 593sec preferred_lft 593sec
        inet6 2001:1c00:e08:faf0:1407:d1b5:c5e:c312/64 scope global dynamic mngtmpaddr noprefixroute 
           valid_lft 593sec preferred_lft 593sec
        inet6 fe80::8df0:5f16:2ece:981c/64 scope link noprefixroute 
           valid_lft forever preferred_lft forever

We use `sysctl -w net.ipv6.conf.eth0.use_tempaddr=2` to enable privacy addresses on Linux. For more information see https://tldp.org/HOWTO/Linux+IPv6-HOWTO/ch06s05.html

### Link local addresses

Every IPv6 interface requires a link local address. The address is typicallly assigned on boot by the interface itself. Unlike global addresses, link local addresses are scoped to a given LAN ("link") and are only accessible to directly connected hosts. They are not routable - a router will drop packets with an LL address.

Link-local addresses have the special  prefix `fe80::/10` with a self-assigned interface id, usually derived from the MAC address. LL addresses  are equivalent to the `169.254/16` IPv4  link-local subnet which is similarly not routable.

Unlike the IPv4 link locals,  IPv6 Link-local addresses serve a very important function in IPv6: they serve as a "bootstrap" address for global-address auto configuration. The link-local address of the router (obtained via Router Advertisement) is also typically used as a host's  default gateway instead of the router's "globally routable" address.

> For communication between hosts in an IPv6 LAN the global address of each host is typically used rather than the LL address of that host. Aside from the default route and global address autoconfiguration, link local addresses are not often used.

**Zone identifier syntax**

Because link-local addresses are scoped to a hardware link if we wish to communicate with an LL address we need to specify the interface its scoped to. There is special syntax for this known as "zone identifier syntax:

    ping fe80::1%eth0

 Where the interface name should appear to the right of the `%`. In the case of `ping`  this syntax is just a short-hand for: `ping -I eth0 fe80::1` but this syntax applies generally, and not just to the `ping` command.

### Unique local addresses

Unique local addresses are equivalent to the IPv4 `10/8`, `172.16/12` and `192.168/1` private networking subnets. They are routable within a WAN but are not routable on the wider internet. An interface is assigned one of these addresses if it wants to be accessible to hosts outside its immediate LAN but does not want to be visible on the internet at large.

The special  prefix for a unique local address is `fc00::/7`

### Multicast addresses

Multicast addresses are a special class of address used to send messages to a subset of IPv6 addresses (the subscribers to a multicast group). They are crucial for "neighbor discovery". They have the prefix: `ff00::/8` 

One important group is the "all routers multicast group". All routers subscribe to this  and listen for "Router Solicitation" packets from hosts. When they detect one they respond with "Router advertisement" packets which contain important network configuration information. The all routers multicast address is `ff02::2`

### Neighbor Discovery

We've now covered all the important IPv6 address types. But how do IPv6 hosts configure themselves and locate other on-link hosts? [Neighbor discovery](https://blog.apnic.net/2019/10/18/how-to-ipv6-neighbor-discovery/) is an ICMPv6 protocol that is responsible for gathering important information for configuring and maintaining an IPv6 network.

 It's comprised of 5 parts:

- Finding local routers
    - aka Router Advertisement / Router Solicitation
- Finding the set of network address prefixes that can be reached via local delivery
    - aka Prefix discovery
- Finding a host's MAC address given its IPv6 address
    - aka Neighbor advertisement / Neighbour Solicitation
    - Equivalent to [ARP](https://en.wikipedia.org/wiki/Address_Resolution_Protocol) in IPv4
        - Though ND is bulit on top of the IP protocol (ICMPv6) but ARP is not.
- Detecting duplicate IP addresses
    - Duplicate Address Detection (DAD)
- Determining that some neighbors are reachable

Neighbor discovery is a fairly complicated protocol that makes extensive use of [ICMPv6](https://en.wikipedia.org/wiki/Internet_Control_Message_Protocol_for_IPv6) and multicast addresses. The details of how it works are not important to us, but we cover it briefly in the next section.

### Address assignment

There are two competing ways for hosts to obtain globally-routable IPv6 addresses: [SLAAC](https://howdoesinternetwork.com/2013/slaac) and [DHCPv6](https://en.wikipedia.org/wiki/DHCPv6). 

**SLAAC (StateLess Address Auto Configuation)**

This is the most common approach found on IPv6 installations. Making use of neighbor discovery an IPv6 host is able to self-assign a global IP. 

It works as follows:

- The IPv6 host begins by self-assigning its link-local address by prepending `fe80::/64` to its interface ID (interface ID is usually generated from the MAC address)
- It sends out a RS (Router solicitation) ICMPv6 packet  to the "all routers" multicast group
- The router responds with an RA packet (Router advertisement) containing a list of prefixes that can be used on the network
- The host then chooses a prefix and appends its interface ID, forming the "globally routable" address.
    - If the router sent multiple prefixes the host may generate multiple addresses
    - The host may also generate globally routable temporary (aka "privacy") addresses with randomized interface ids
- The RA packet also contains the DNS servers and the router's link-local address; which the host uses to sets the default gateway

There are many more details we didn't cover, but at the end of this process the host will have: 

(1) A link-local address 

(2) At least one globally routable IP 

(3) A list of IPv6 DNS servers 

(4) The address of the default gateway (router)

This is enough for the host to begin communicating with the outside world. 

**DHCPv6**

DHCPv6 is an alternative to SLAAC, and less likely to be found in the wild. Unlike SLAAC, a DHCPv6 server will tell a host its entire 128 bit IP. It does not allow the host to autoconfigure its "interface id" or select which network prefix to use.

It works as follows:

- Host begins by sending a DHCPv6 request to DHCP multicast: `ff02::1:2`
- DHCPv6 response contains a list of (perhaps 1) IPv6 addresses
    - each address has an expration date
- DHCPv6 server must allocate addresses sparsely, not consecutively, so an attacker cannot guess other addresses given one address

One difference between DHCPv6 and SLAAC is that DHCPv6 assigns the host a `/128` address; it does not indicate to the host the network prefix length ( even though the network prefix length is most likely `64` bits.)

## Desktop IPv6 Integration

Now we have an overview of how IPv6 operates, we can investigate how supporting it will impact the desktop client.  First we must ask what it means to support IPv6. At the least it means the user is able to browse and access IPv6 hosts. For some it may also mean the user connects to our VPN server via IPv6 too. Depending on whether the user connects to our VPN server via IPv4 or IPv6 a different set of changes are needed. 

  Introducing support for IPv6 will at least touch the following components of desktop:

- [VPN connectivity](#vpn-connectivity-and-routing)
- [Firewall](#firewall)
- [DNS](#dns)
- [Handshake](#handshake)
- [Split tunnel](#split-tunnel)

### VPN Connectivity and Routing

**OpenVPN**

OpenVPN has had IPv6 support since version `2.3.0`; and the client configuration changes to support it [are straightforward](https://community.openvpn.net/openvpn/wiki/IPv6). Nonetheless the following routing changes will need to happen when we introduce IPv6 support (even if it not directly performed by us):

- First we need to ensure there are both IPv4 and IPv6 ips set on the tunnel device
- Secondly we need a rule in the IPv6 routing table to set the `tun` device as the default gateway.
    - In our IPv4 routing table we use the `0/1` and `128/1` tricks to ensure route priority, the IPv6 equivalent of these rules are:

    `ip -6 route add ::/1 via <tunnel ip6>`  

    `ip -6 route add 8000::/1 via <tunnel ip6>`

- Third we need a route to the VPN server itself.
    - If the user is connected to our server on IPv4, this route will be in the IPv4 routing table:
        - `ip route add <vpn ip> <physical gateway>`
        - There is no route required in the IPv6 routing table since IPv6 traffic will be sent through the tunnel and come out wrapped in IPv4 packets.
    - However, if the user is connected to our server on IPv6, this route will be in the IPv6 routing table instead:
        - `ip -6 route add <vpn ip> <physical gateway>`
        - No route is required in the IPv4 routing table (the Ipv4 traffic will be wrapped in IPv6 packets)

**WireGuard**

WireGuard on IPv6 should mostly "just work" on the client, with just  a few small routing changes:

**Linux**

On Linux, we use  `fwmark` together with a routing policy to manage our routing, [see here](https://www.wireguard.com/netns/#improved-rule-based-routing).

All that should be necessary is introducing this same routing approach into the `ip6tables` firewall and the IPv6 routing table. No further work should be required. 

This is because if the user is connected to our VPN server over IPv4 then the `fwmark` will never appear in IPv6 packets so everything will get sent through the `wireguard` interface and come out wrapped in IPv4 packets. These wrapped IPv4 packets will have the `fwmark` and they'll get routed out the IPv4 routing table according to the routing policy - which will send them out the physical interface. Vice versa in the case the user is connected to one of our IPv6 servers.

**Macos and Windows**

We reproduce the same approach we used for OpenVPN but with the wireguard interface.

### Firewall

The only firewall rules directly impacted by IPv6 support are the `allow LAN` rules and DNS, DNS will be discussed in its own section. 

**Allow LAN**

We do currently offer some support for IPv6 `allow LAN` but it is incomplete. We whitelist the link local, unique local and multicast address ranges. However this is not how on-link IPv6 devices primarily communicate - they use the globally routable IP addresses. 

As discussed earlier, all on-link IPv6 hosts have the same network prefix in their global IPs. 

Gven a globally routable IP of `2001:1c00:e08:faf0:1407:d1b5:c5e:c312`

We take the first 64 bits and construct the following rule to whitelist all on-link IPv6 traffic:

`ip6tables -A piavpn.320.allowDNS -d 2001:1c00:e08:faf0::/64 -j ACCEPT`

Alternatively, since the `/64` automatically takes the first 64 bits we can simply use the complete global IP for the host (no need to explicitly take the first 64 bits ourselves), e.g

`ip6tables -A piavpn.320.allowDNS -d 2001:1c00:e08:faf0:1407:d1b5:c5e:c312/64 -j ACCEPT`

To find the globally routable IP we can use `ip addr show eth0` and look for the IPv6 IP with scope `global`:

    $ ip addr show eth0
    3: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
        link/ether 94:e6:f7:aa:25:30 brd ff:ff:ff:ff:ff:ff
        inet 192.168.1.204/24 brd 192.168.1.255 scope global dynamic noprefixroute eth0
           valid_lft 85356sec preferred_lft 85356sec
        inet6 2001:1c00:e08:faf0:4812:9efd:fc9a:ed85/64 scope global temporary dynamic 
           valid_lft 594sec preferred_lft 594sec
        inet6 2001:1c00:e08:faf0:1407:d1b5:c5e:c312/64 scope global dynamic mngtmpaddr noprefixroute 
           valid_lft 594sec preferred_lft 594sec
        inet6 fe80::8df0:5f16:2ece:981c/64 scope link noprefixroute 
           valid_lft forever preferred_lft forever

> Note the "global temporary dynamic" addresses above - these are the "privacy addresses" mentioned earlier in the "globally routable addresses" section.

Similarly on Windows, we look for the `IPv6 Address` or `Temporary IPv6 Address` (both are fine) to find the global IP:

    IPv6 Address. . . . . . . . . . . : 2601:800:cGenerally the network prefix is 64 bits so we can simply take the first 64 bits of the host's global IP and construct the following firewall rule, e.g100:5f4:8dfd:b814:46ec:1cd0(Preferred) 
    Temporary IPv6 Address. . . . . . : 2601:800:c100:5f4:d520:c5f6:2289:e87d(Preferred) 
    Link-local IPv6 Address . . . . . : fe80::8dfd:b814:46ec:1cd0%15(Preferred)

On MaOS it is similarly easy to find the global IPs.

**What if the network prefix isn't 64 bits?**

The network prefix is defined  by standard to be `/64` ([see here](https://en.wikipedia.org/wiki/IPv6_address#Address_formats)) so this approach should be completely reliable, however we can also find the network prefix directly by looking for the "on link" IPv6 prefix from the routing table:

    $ ip -6 route show via ::
    ::1 dev lo proto kernel metric 256 pref medium
    2001:1c00:e08:faf0::/64 dev eth0 proto ra metric 600 pref medium
    fe80::/64 dev eth0 proto kernel metric 600 pref medium

Note the `2001:1c00:e08:faf0::/64` prefix given above.

**Caveats**

Because the Network Prefix is defined by the particular IPv6 network the host is connected to (unlike the link local `fe80::/10` prefix), we need to update the firewall rule whenever the user changes network.

**What about Unique local addresses?**

We already whitelist "unique local" addresses (the IPv6 equivalent of IPv4 private network subnets, e.g `192.168/16`) as we whitelist the `fc00::/7` IPv6 subnet.

**Other IPv6 Firewall considerations**

Since IPv6 addresses are globally accessible there is no NAT layer. This means that a listening socket on an IPv6 host is directly accessible to the outside world (whereas on IPv4 we'd have to explictly forward a port in the router). This appears concerning at first, however our IPv6 firewall rules will block all non VPN off-link outbound traffic via the Killswitch (just as in the IPv4 firewall) - so though we may receive incoming connection attempts these will remain unresponded to and so it's unnecessary to explicitly block incoming packets.

### DNS

We will need to implement the same `blockDNS` and `allowDNS` rules on IPv6 firewall as IPv4. However one quirk is supporting "Custom DNS" for mixed IPv4 and IPv6 DNS servers. In this case we'll need to detect the IP address type and update the rules in the appropriate firewall. 

Similarly "Existing DNS" can be implemented by disabling the `blockDNS` rule on both the IPv4 and IPv6 firewalls.

### Handshake

It is unclear whether it's worthwhile to support a local IPv6 handshake resolver. Since IPv4 and IPv6 DNS servers should be able to resolve domains for both IPv4 and IPv6 ips, implementing a local IPv6 DNS server is probably unnecessary.

In our current implementation we setup a local handshake nameserver in an alternative address on the `127/8` loopback subnet: `127.80.73.65` to avoid conflicts with existing DNS proxies found at `127.0.0.1:53`

Unfortunately IPv6 only has one loopback address (`::1`), not a subnet, so if there's an existing DNS server at `::1:53` we cannot avoid conflict by binding to a different loopback subnet IP. However IPv6 allows us to add any number of link-local IPs to the loopback interface:

    sudo ip addr add fe80::6/128 dev lo
    
    $ ip addr show lo
    1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
        link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
        inet 127.0.0.1/8 scope host lo
           valid_lft forever preferred_lft forever
        inet6 fe80::6/128 scope link 
           valid_lft forever preferred_lft forever
        inet6 ::1/128 scope host 
           valid_lft forever preferred_lft forever

The only quirk is that binding a listening server to this link local address is slightly tricky: [see here](https://stackoverflow.com/questions/2455762/why-cant-i-bind-ipv6-socket-to-a-linklocal-address)

Also remember that link-local addresses are local to the link - and so `fe80::6` (above) is only accessible on the loopback interface, it's not accessible on the LAN generally.

### Split tunnel

With the exception of Linux the split tunnel  works by rewriting source ips of outgoing packets, this utilizies the [strong host model](https://www.notion.so/IPv6-and-Desktop-f20538998d654218a457f34ff0f505c8#ea586b5b1cde44e9b5b69524a0270a27) to ensure packets go out the interface with that IP. On IPv4 we bypass the VPN by rewriting the source IP of packets to the physical interface IP. This should work exactly the same on IPv6 - we simply grab the IPv6 IP of the physical interface. Likewise for vpnOnly splitting, we can lookup and use the IPv6 IP of the tunnel device for outgoing traffic.

**Windows**:

IPv6 also supports the strong host model and so rewriting the source IP of sockets (to the global IP of the host) should also work to split IPv6 traffic. 

We will need to modify the WFP callout driver to accept the globally routable IPv6 address in the provider context and also modify it to work on the `FWPS_LAYER_ALE_CONNECT_REDIRECT_V6` and `FWPS_LAYER_ALE_CONNECT_REDIRECT_V6`  but these should all be relatively straight forward changes.

**MacOS**:

We will need to introduce (or tweak) IPv6 equivalents of the current IPv4 callbacks in the Kernel extension. Since MacOS also supports the strong host model, providing IPv6 support should be relatively straight forward - especially after we introduce packet based rewriting which should simplify the current code.

Currently the KextClient responds to process queries from the Kext with the IPv4 bypass or vpnOnly addresses. To support IPv6 we need the Kext query object to indicate whether the socket is IPv4 or IPv6 so the KextClient returns the correct `bind_ip` to the Kext.

We will also need to add a firewall `allow` rule for the physical interface IPv6 source IP.

**Linux**:

We do not need to introduce any new cgroups specific to IPv6, however we need to add a firewall rule to the IPv6 firewall to apply our custom `fwmark` to split IPv6 traffic:

`ip6tables -A piavpn.100.tagPkts -m cgroup --cgroup %1 -j MARK --set-mark OUR_MARK`

This mark will be utilised by our `ipv6` routing policies, which we also need to define:

`sudo ip -6 rule add from all fwmark OUR_MARK lookup SPLIT_TABLE`

This table will contain a rule to send all traffic out the default interface (which will be the IPv6 link-local address of the router)

The vpnOnly cgroup is managed similarly.

The flow will be like this:

PID added to cgroup 
→ Process starts sending traffic  

→ IPv6 traffic is managed by the IPv6 firewall, IPv4 traffic by the IPv4 firewall 

→ These rules detect the cgroup and mark packets with `fwmark` 

→ These `fwmark` are utilized by the separate IPv4 and IPv6 routing policies

→ These policies determine which interface the packets go out - whether vpnOnly or bypass

 
> The IPv6 firewall and routing policies do not conflict - even if the `fwmark` is the same. This is because IPv6 and IPv4 firewall and routing live on completely different networking stacks. 

## Conclusion

Supporting IPv6 on Desktop is not unsurmountable and much of the code (such as the firewall code) is already written in a way that anticipates IPv6.