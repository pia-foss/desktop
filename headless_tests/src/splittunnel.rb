require 'resolv'
require_relative 'command'
require_relative 'retry'

class SplitTunnel
    IP_CHECK_SERVICE_URL = "api.ipify.org"

    def self.resolve_url_for_split_tunnelling
        # This method queries an ip checker with multiple ips, 
        # so that we can use one to bypass the vpn and one as a control
        addresses = Retriable.run(attempts: 3, delay: 0.75, expect: lambda {|output| output.length >= 2}) {
            puts "Finding IP addresses for #{IP_CHECK_SERVICE_URL}..."
            Resolv.getaddresses(IP_CHECK_SERVICE_URL)
        }
        if addresses.length < 2
            raise "Unexpected response from #{IP_CHECK_SERVICE_URL}\n" +
            "expected at least 2 ip addresses, got #{result.count}:\n" + result
        end
        return addresses[0], addresses[1]
    end

    def self.curl_for_ip(route)
        puts "curl #{IP_CHECK_SERVICE_URL} via #{route}..."
        output = Retriable.run(attempts: 3, delay: 0.75) { Command.execute_with_output("curl #{IP_CHECK_SERVICE_URL} --resolve *:80:#{route} --connect-timeout 30") }
        puts "Response: #{output}"
        output
    end

    def self.setup_split_tunnel_default_route(default_tunnel_action)
        puts "Enabling split tunneling..."
        PiaCtl.set_unstable("splitTunnelEnabled", true)
        puts "Setting Split tunneling to #{default_tunnel_action} by default."
        default_route = default_tunnel_action == :useVpn ? true : false
        PiaCtl.set_unstable("defaultRoute", default_route)
    end

    def self.bypass_vpn_for_ip(ip_address)
        puts "Setting #{ip_address} to bypass the VPN..."
        settings = [{mode: "exclude", subnet: ip_address}]
        PiaCtl.set_unstable("bypassSubnets", settings)
    end
end
