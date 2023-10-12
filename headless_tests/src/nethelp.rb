require_relative 'checksystem'
require 'ipaddr'

# Helper class for networking constants, Ip addresses, etc.
class NetHelp 
    PRIVATE_SUBNET_RANGES = [
        IPAddr.new("10.0.0.0/8"),
        IPAddr.new("172.16.0.0/12"),
        IPAddr.new("192.168.0.0/16")
    ]
    
    # Return true if an address complies with rfc1918.
    # In other words, true if it's a private LAN address.
    def self.rfc1918?(ip_address)
        ip = IPAddr.new(ip_address)
        PRIVATE_SUBNET_RANGES.any? { |range| range.include?(ip) }
    end

    # Valid network system sources outside of the VPN.
    # On linux this will be an array of available interfaces, while on mac
    # and windows it will be an array of valid local source IP addresses.
    # These can be used as the source of an outgoing package in leak tests. 
    # If they  the outside world the system is leaking its real IP address.
    def self.get_valid_sources
        case check_system
        when :linux
            # Get all interfaces that are not a VPN tunnel or loopback
            Socket.getifaddrs.map { |ifaddr| ifaddr.name }.uniq.select { |ifname| !ifname.start_with?("tun", "wgpia", "lo") }
        else
            Socket.ip_address_list.select {|ip_address| ip_address.ipv4? }.map { |ip_address| ip_address.inspect_sockaddr }.select { |address| rfc1918? address }
        end
    end

    # function used in allowlan tests to get the default gateway IP address
    def self.get_default_gateway_ip
        case check_system
        when :windows
            get_gateway = `ipconfig | findstr "Default Gateway"`
            gateway_match = /Default Gateway . . . . . . . . . : (\S+)/.match(get_gateway)
        when :linux
            get_gateway = `ip -4 route | grep default`
            gateway_match = /default via (\S+)/.match(get_gateway)
        else #macOS
            get_gateway = `netstat -nr -f inet | grep default | grep UGScg`
            gateway_match = /default            (\S+)/.match(get_gateway)
        end

        if gateway_match
            return gateway_match[1]
        else
            return nil
        end
    end
end
