require_relative 'systemutil'
require 'ipaddr'
require 'socket'
require 'open3'

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

    def self.default_nameservers
        ip_addresses = []
        nslu_output = `nslookup privateinternetaccess.com 2>&1`
        nslu_output.each_line do |line|
            if line =~ /Address:[\t ]*(\d+\.\d+\.\d+\.\d+)/
                ip_addresses << $1
            end
        end

        # Specify the path to the resolv.conf file
        resolv_conf_path = '/etc/resolv.conf'
        # in ubuntu /etc/resolv/conf doesn't update so need to go to the root
        if File.exist?('/run/systemd/resolve/resolv.conf')
            resolv_conf_path = '/run/systemd/resolve/resolv.conf'
        end

        # Check if the file exists
        if File.exist?(resolv_conf_path)
            # Open and read the file line by line
            File.open(resolv_conf_path, 'r') do |file|
                file.each_line do |line|
                    # Use a regular expression to match lines containing "nameserver"
                    if line =~ /^nameserver\s+(\S+)/
                        ip_addresses << $1
                    end
                end
            end
        end
        ip_addresses
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

    def self.pia_connected?
        output, errout, status = Open3.capture3('curl https://www.privateinternetaccess.com/api/client/status')
        raise "Could not test connection: #{errout}" if status != 0
        JSON.parse(output)['connected']
    end

    def self.curl_for_ip
        # We want to check the ip against an external source, as otherwise we are essentially comparing our internal status api against itself.
        # To avoid rate limits and reduce risk of occasional downtime for specific site, pick from a list. 
        sites = ["https://api.ipify.org?format=json", "https://api.myip.com", "https://ipapi.co/json", "https://jsonip.com/", "https://api.seeip.org/jsonip?", "https://ifconfig.co/json"]
        output, errout, status = Open3.capture3("curl \"#{sites.sample}\"")
        if output.include? '"ip"'
            ip = JSON.parse(output)['ip']
        else
            status = 1
        end
        raise "Could not test connection: #{errout}" if status != 0
        ip
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
