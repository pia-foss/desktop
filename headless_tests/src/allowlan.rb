require_relative 'checksystem'
require_relative 'command'

class AllowLan
    def self.build_ping_command(address, n: 1)
        case check_system
        when :windows
            return "ping #{address} -n #{n}"
        else
            return "ping #{address} -c #{n}"
        end
    end
    
    def self.ping_with_retry?(address, expected_result, n: 10)
        ping_cmd = build_ping_command(address)
        return Command.execute_with_retry?(ping_cmd, expected_result)
    end

    def self.nslookup_with_retry?(address, expected_result, n: 10)
        nslookup_cmd = "nslookup google.com #{address}"
        return Command.execute_with_retry?(nslookup_cmd, expected_result)
    end

    # This function is needed because on Windows
    # nslookup exit code is always zero, even when
    # it fails due to a timeout error.
    # The exit code is non-zero on both Linux and macOS 
    # in that scenario. (Why is it always you MS?)
    def self.win_nslookup_with_retry?(address, expected_result, n: 10)
        nslookup_cmd = "nslookup google.com #{address}"
        puts "Running `#{nslookup_cmd}` #{n} times expecting this result: #{expected_result}"
        n.times {
            nslookup_output = `#{nslookup_cmd}`
            result = nslookup_output.include?("google.com")
            if result == expected_result
                return expected_result
            end
            sleep(0.75)
        }
        return !expected_result
    end

    # The reason why we test using a different method for 
    # different platform is that:
    # Linux: CI uses Docker which allows ping. However, nslookup is not allowed. 
    #        This is because Docker NAT default gateway does not run a DNS server.
    # macOS: pings are blocked in CI (Github-hosted machines), so we use nslookup.
    # Windows: since headless tests are not running in Github-hosted machines, 
    #          but in our own VMs infrastructure, ping is not blocked.
    def self.test_local_traffic(local_ip, expected_result)
        case check_system
        when :windows
            result = ping_with_retry?(local_ip, expected_result)
        when :linux
            result = ping_with_retry?(local_ip, expected_result)
        else #macOS
            result = nslookup_with_retry?(local_ip, expected_result)
        end
        return result
    end
end
