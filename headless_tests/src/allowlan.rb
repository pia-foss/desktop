require_relative 'systemutil'
require_relative 'command'

class AllowLan
    def self.build_local_traffic_test_command(address)
        case SystemUtil.os
        when :windows
            # since headless tests are not running in Github-hosted machines, 
            # but in our own VMs infrastructure, ping is not blocked.
            "ping #{address} -n 1"
        when :linux
            # CI uses Docker which allows ping. However, nslookup is not allowed. 
            # This is because Docker NAT default gateway does not run a DNS server.
            "ping #{address} -c 1"
        else
            # macOS: pings are blocked in CI (Github-hosted machines), so we use nslookup.
            "nslookup google.com #{address}"
        end
    end

    def self.test_local_traffic(local_ip)
        command_to_run = build_local_traffic_test_command(local_ip)
        Command.execute(command_to_run)
    end
end
