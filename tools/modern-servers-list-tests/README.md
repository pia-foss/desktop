# Modern servers list tests

This directory contains example modern regions lists that can be tested with overrides.

The modern regions list is designed to be flexible, in particular it allows Ops to create/combine/break/destroy server groups at any time - the clients should depend only on the services listed, not the groups.  These examples can be used to test that.

The purpose of the groups in the new format is just to reduce the redundancy of the JSON (significantly).  In a complete model, the servers could each individually list their offered services and ports.  However, this JSON format is very large and repetitive, in reality servers belong to one of a handful of groups that have exactly the same deployment.  The common information is therefore extracted to the server "groups", and only per-server information is listed with each server.

## Normal Examples

The "US California" location is renamed in each example to indicate which override is active.

### normal_bridge.json

This is a normal example that uses legacy servers written in the new format (it "bridges" the legacy infrastructure to the new servers list).

### normal_renamed_groups.json

This example simply renames the groups, to ensure that clients do not depend on the specific group names.

### normal_split_groups.json

This example breaks up all the groups, except for latency.  Clients shouldn't rely on various services being provided by the same servers - Ops is free to combine or break services apart at any time.  This may become more common in the future in order to support TCP 443 for more services, etc.

Clients can rely on the "latency" service being present on all servers in the modern infrastructure (although Desktop does not).

### normal_favored.json

This example has different combinations of ports on different servers.  All VPN servers offer OpenVPN TCP, OpenVPN UDP, and WireGuard, but some ports are only available on some servers.

This is as if we want to provide TCP 443 for all services, so it has a different use on different servers.  (TCP 443 is not actually available for anything other than OpenVPN right now though, so this can't actually be modeled yet.)

### normal_regional_port.json

This example has a "regional port" - a port that is added in a specific region, such as to circumvent a specific national firewall.

Since no regional ports actually exist today, OpenVPN UDP 9201 and TCP 110 are modeled as regional ports for Sweden - they are only available in this region.

### normal_new_service.json

This example contains new (made-up) services that have completely different information from existing services.  Clients should tolerate new services properly, whether they're grouped with existing services or not (ignore them, use the known services).

### normal_regional_defaults.json

This example varies the default ports in different regions and servers (the first port in the service's list).

Connections using the "default" setting should use the default port for that server.  Connections using a specific port can use any server that supports the port, regardless of whether it's the default.

The changes made are:

* US California has a default of 9201 on all servers for OpenVPN UDP
* CA Toronto's defaults vary by server - some are 1194, some are 53
* The default remains 8080 for OpenVPN UDP elsewhere

## Error Examples

TODO - Need to write some malformed examples to verify failure modes (malformed servers should be ignored while still loading remaining servers, etc.)
