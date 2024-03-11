require 'resolv'
require_relative 'command'
require_relative 'retry'

class SplitTunnel
    IP_CHECK_SERVICE_URL = "api.privateinternetaccess.com"
    IP_CHECK_SERVICE_URL_PATH = "#{IP_CHECK_SERVICE_URL}/api/client/status"

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

    def self.curl_for_ip_via_route(route)
        puts "curl #{IP_CHECK_SERVICE_URL_PATH} via #{route}..."
        output = Retriable.run(attempts: 3, delay: 0.75) { Command.execute_with_output("curl #{IP_CHECK_SERVICE_URL_PATH} --resolve *:80:#{route} --connect-timeout 30") }
        puts "Response: #{output}"
        JSON.parse(output)['ip']
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

    def self.set_app_split_tunnel_rule(os)
        if os == :windows
            only_vpn_path = self.find_wget
            where_curl = Command.execute_with_output("where curl").strip
            bypsass_vpn_path = where_curl.split("\n")[0]
        elsif os == :linux
            python_symlink  = Command.execute_with_output("which python3").strip
            only_vpn_path = Command.execute_with_output("readlink -f #{python_symlink}").strip
            bypsass_vpn_path = Command.execute_with_output("which curl").strip
        end

        only_vpn_rule = {linkTarget: "", mode: "include", path: "#{only_vpn_path}"}
        bypass_vpn_rule = {linkTarget: "", mode: "exclude", path: "#{bypsass_vpn_path}"}
        settings = [only_vpn_rule, bypass_vpn_rule]
        PiaCtl.set_unstable("splitTunnelRules", settings)
    end

    def self.get_queries(os)
        if os == :windows
            wget_path = self.find_wget
            only_vpn_query = "#{wget_path} -T 20 -t 5 -qO - #{IP_CHECK_SERVICE_URL_PATH}"
        elsif os == :linux
            only_vpn_query = "python3 -c \"import requests, time; time.sleep(1); print(requests.get(\'https://api.privateinternetaccess.com/api/client/status\').text)\""
        end
        bypass_vpn_query = "curl #{IP_CHECK_SERVICE_URL_PATH} --connect-timeout 10"
        control_query = "ruby -rnet/http -e \'puts Net::HTTP.get_response(URI(\"https://#{IP_CHECK_SERVICE_URL_PATH}\")).body\'"
        [only_vpn_query, bypass_vpn_query, control_query]
    end

    def self.check_ip(query)
        puts "Querying #{query}"
        output = Command.execute_with_output(query)
        JSON.parse(output.strip)['ip']
    end

    def self.can_connect(query)
        puts "Trying to connect via #{query}"
        Command.execute(query)
    end

    def self.find_wget
        ruby = Command.execute_with_output("where ruby")
        ruby_base = ruby.slice(0..(ruby.index('\bin')))
        Command.execute_with_output("where /R #{ruby_base} wget.exe").strip
    end
end