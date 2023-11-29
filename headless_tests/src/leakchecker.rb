require_relative 'systemutil'
require_relative 'nethelp'

# Helper class to test for leaks.
class LeakChecker
    LEAK_CHECK_METHODS = [:ping, :curl]

    def initialize
        @source_endpoints = NetHelp.get_valid_sources
    end

    def available_methods
        # Retrieve a list of method that are leaking
        # given our current system configuration
        LEAK_CHECK_METHODS.select { |method| leaks? method }
    end

    def leaks?(method)
        case method
        when :ping
            ping_leaks?
        when :curl
            curl_leaks?
        end
    end

    # Checks if it is possible to reach an external IP
    # by forcing ping to use private LAN addresses as the source.
    # If this is possible, the firewall rules in the system allow
    # our real address to "leak" to the outside world.
    def ping_leaks?
        @source_endpoints.each do |source|
            extra_options = []
            case SystemUtil.os
            when :linux
                source_option = "-I"
                count_option = "-c"
                extra_options = ["-B"]
            when :macos
                source_option = "-S"
                count_option = "-c"
            when :windows
                source_option = "-S"
                count_option = "-n"
            end
            ping_cmd = "ping #{source_option} #{source} 1.1.1.1 #{count_option} 1 #{extra_options.join(' ')}"
            system ping_cmd, :out => File::NULL, :err => File::NULL
            # If the ping command is successful, we have a leak
            if $?.exitstatus == 0
                return true
            end
        end
        false
    end

    # Checks if it is possible to reach an external IP
    # by forcing curl to use a specified source for the connection outside the VPN.
    # If this is possible, the firewall rules in the system allow
    # our real address to "leak" to the outside world.
    def curl_leaks?
        # Valid system sources for packages.
        # On linux this will be an array of available interfaces, while on mac
        # and windows it will be an array of
        @source_endpoints.each do |source|
            curl_cmd = "curl --interface #{source} --ipv4 http://ipinfo.io/ip --connect-timeout 2"
            system curl_cmd, :out => File::NULL, :err => File::NULL
            # If the curl command is successful, we have a leak
            if $?.exitstatus == 0
                return true
            end
        end
        false
    end
end
