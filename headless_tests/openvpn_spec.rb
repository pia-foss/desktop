require_relative 'src/piactl'
require_relative 'src/nethelp'
require_relative 'src/retry'
require_relative 'src/systemutil'

describe "OpenVPN options" do
    before(:each) do
        PiaCtl.set("protocol", "openvpn")
    end

    def connect_and_check
        Retriable.run(attempts: 2, delay: 1, expect: true) {
            PiaCtl.connect
            # Wait to avoid a false positive in the first check
            vpn_ip = PiaCtl.get_vpn_ip
            # Check the connection, may fail at first due to the new connection
            connected = Retriable.run(attempts: 2, delay: 3, expect: true) {
                NetHelp.pia_connected?
            }
            PiaCtl.disconnect
            if !connected
                puts "Failed to connect with IP #{vpn_ip}"
            end
            connected
        }
    end

    ["AES-128-GCM", "AES-256-GCM"].each do |cipher|
        describe "OpenVPN with cipher #{cipher}", :openVPNOnly do
            it "can connect" do
                PiaCtl.set_unstable("cipher", cipher)
                pia_connected = connect_and_check
                expect(pia_connected).to be_truthy, "PIA Failed to connect"
            end
        end
    end

    describe "OpenVPN UDP remote port", :openVPNOnly do
        upd_test_ports = [853, 8080]
        # Port 53 does not work on MacOS, skip.
        upd_test_ports << 53 if !SystemUtil.macos?
        # Port 123 does not work in CI infrastructure
        upd_test_ports << 123 if !SystemUtil.CI?
        upd_test_ports.each do |port|
            it "#{port} can connect" do
                PiaCtl.set_unstable("protocol", "udp")
                PiaCtl.set_unstable("remotePortUDP", port)
                expect(connect_and_check).to be_truthy, "PIA Failed to connect"
            end
        end
    end

    describe "OpenVPN TCP remote port", :openVPNOnly do
        tcp_test_ports = [8443, 443, 853, 80]
        tcp_test_ports.each do |port|
            it "#{port} can connect" do
                PiaCtl.set_unstable("protocol", "tcp")
                PiaCtl.set_unstable("remotePortTCP", port)
                pia_connected = connect_and_check
                expect(pia_connected).to be_truthy, "PIA Failed to connect"
            end
        end
    end
end
