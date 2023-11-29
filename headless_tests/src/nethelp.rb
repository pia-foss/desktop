require_relative 'systemutil'
require 'ipaddr'
require 'socket'

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
        if SystemUtil.linux?
            get_valid_interfaces
        else
            get_valid_addresses
        end
    end

    def self.get_valid_interfaces
        # Get all interfaces that are not a VPN tunnel or loopback
        Socket.getifaddrs.map(&:name).uniq.reject { |ifname| ifname.start_with?("tun", "wgpia", "lo") }
    end

    def self.get_valid_addresses
        Socket.ip_address_list.select(&:ipv4?).map(&:inspect_sockaddr).select { |address| rfc1918? address }
    end

    # function used in allowlan tests to get the default gateway IP address
    def self.get_default_gateway_ip
        case SystemUtil.os
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
            gateway_match[1]
        else 
            nil
        end
    end

    def self.get_default_nameserver
        nslu_output = `nslookup privateinternetaccess.com 2>&1`
        nslu_output.each_line do |line|
            if line =~ /Address:[\t ]*(\d+\.\d+\.\d+\.\d+)/  
                return $1
            end
        end
    end

    def self.can_send_message_externally?
        messenger_lambda = ENV["PIA_AWS_SEND_MESSAGE_LAMBDA"]
        AWSLambda.has_credentials? && messenger_lambda
    end

    def self.send_message_externally(address, port, message)
        messenger_lambda = ENV["PIA_AWS_SEND_MESSAGE_LAMBDA"]
        lambda_client = AWSLambda.new
        lambda_client.invoke(messenger_lambda, {ip: address, port: port, message: message})
    end

    class SimpleMessageReceiver
        def initialize(port)
            @message = nil
            @server = TCPServer.new('0.0.0.0', port)
            @server_thread = Thread.new do
                client_sock = nil
                client_sock = @server.accept
                @message = client_sock.readpartial(1024)
            ensure
                client_sock.close if client_sock != nil
            end
        end

        def message_received?
            @message != nil
        end

        def message
            @message
        end

        def cleanup
            @server.close
            @server_thread.join
        end

        def kill_and_cleanup
            @server_thread.kill
            cleanup
        end
    end

    class SimpleMessageSender
        # Attempts to send a message. It will time out with no errors nor indication.
        def self.send(address, port, message, local_host, timeout=1)
            begin
                socket = Timeout::timeout(timeout) do
                    client_socket = TCPSocket.new(address, port, local_host)
                    client_socket.write(message)
                ensure
                    client_socket.close
                end
            rescue Timeout::Error
            end
        end
    end
end
