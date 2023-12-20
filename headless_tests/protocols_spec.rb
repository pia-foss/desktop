require_relative 'src/piactl.rb'
require_relative 'src/nethelp'
require_relative 'src/retry'

describe "Conecting with different protocol settings" do
    describe "when switching between different prototcols" do
        ["udp", "tcp"].each do |transport|
            it "connects successfully using openvpn - #{transport}", :openVPNOnly do
                PiaCtl.set("protocol", "openvpn")
                PiaCtl.set_unstable("protocol", transport)
                PiaCtl.connect
                connected = Retriable.run(attempts: 3, delay: 3, expect: true) {
                    NetHelp.pia_connected?
                }
                expect(connected).to be_truthy, "Failed to connect to openvpn - #{transport}"
            end
        end
        it "connects successfully using wireguard", :wireguardOnly do
            PiaCtl.set("protocol", "wireguard")
            PiaCtl.connect
            connected = Retriable.run(attempts: 3, delay: 3, expect: true) {
                NetHelp.pia_connected?
            }
            expect(connected).to be_truthy, "Failed to connect to wireguard"
        end
    end
    describe "when selecting different packet sizes" do
        test_cases = [
            {packet_size: "smallPackets", mtu: 1250, protocol: "openvpn"},
            {packet_size: "smallPackets", mtu: 1250, protocol: "wireguard"},
            {packet_size: "largePackets", mtu: 0, protocol: "openvpn"},
            {packet_size: "largePackets", mtu: 0, protocol: "wireguard"}
        ]
        test_cases.each do |test_case|
            it "connects successfully when #{test_case[:packet_size]} is selected" do
                PiaCtl.set("protocol", test_case[:protocol])
                PiaCtl.set_unstable("mtu", test_case[:mtu])
                PiaCtl.connect
                connected = Retriable.run(attempts: 3, delay: 3, expect: true) {
                    NetHelp.pia_connected?
                }
                expect(connected).to be_truthy, "Failed to connect with #{test_case[:packet_size]} selected"
            end
        end
    end
end
