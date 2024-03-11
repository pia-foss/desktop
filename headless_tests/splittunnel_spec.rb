require_relative 'src/piactl'
require_relative 'src/splittunnel'
require_relative 'src/systemutil'
require_relative 'src/nethelp'

describe "Split tunnel" do
    before(:all) do
        if SystemUtil.macos?
            skip("Split tunnel does not work on MacOS")
        end
        @os = SystemUtil.os
        @only_vpn_query, @bypass_vpn_query, @control_query = SplitTunnel.get_queries(@os)
    end
    [:bypassVpn,:useVpn].each do |defaultTunnelAction|
    # Loop through twice with the different default split tunnel states
        describe "when split tunneling by IP (#{defaultTunnelAction})" do
            before(:all) do
                # Switch on split tunneling with all apps using vpn by default
                SplitTunnel.setup_split_tunnel_default_route(defaultTunnelAction)
                @use_vpn_ip, @bypass_vpn_ip = SplitTunnel.resolve_url_for_split_tunnelling
                SplitTunnel.bypass_vpn_for_ip("#{@bypass_vpn_ip}")
                PiaCtl.connect
                @vpn_ip = PiaCtl.get_vpn_ip
            end
        
            it "IP saved in settings will bypassVpn, otherwise #{defaultTunnelAction}", :aggregate_failures do
                use_vpn_curl_result = SplitTunnel.curl_for_ip_via_route(@use_vpn_ip)
                # Depends on whether split tunnel is set to bypass/use VPN by default
                if defaultTunnelAction == :useVpn
                    expect(use_vpn_curl_result).to eq(@vpn_ip)
                elsif defaultTunnelAction == :bypassVpn
                    expect(use_vpn_curl_result).not_to eq(@vpn_ip)
                end

                bypass_vpn_curl_result = SplitTunnel.curl_for_ip_via_route(@bypass_vpn_ip)
                # Should always bypass regardless of default setting
                expect(bypass_vpn_curl_result).not_to eq(@vpn_ip)
            end
        end
        describe "When split tunneling by app (#{defaultTunnelAction})" do
            it "When connected to VPN", :aggregate_failures do
                SplitTunnel.setup_split_tunnel_default_route(defaultTunnelAction)
                SplitTunnel.set_app_split_tunnel_rule(@os)
                PiaCtl.connect
    
                vpn_ip = PiaCtl.get_vpn_ip
    
                # An app not specified in split tunnel rules should follow default rules
                if defaultTunnelAction == :useVpn
                    control_result = Retriable.run(attempts: 5, delay: 1, expect: vpn_ip) { SplitTunnel.check_ip(@control_query) }
                    expect(control_result).to eq(vpn_ip)
                elsif defaultTunnelAction == :bypassVpn
                    control_result = Retriable.run(attempts: 5, delay: 1, expect: proc {|result| result != vpn_ip}) { SplitTunnel.check_ip(@control_query)}
                    expect(control_result).not_to eq(vpn_ip)
                end

                # An app added to use "Only VPN" should always connect to VPN
                only_vpn_result = Retriable.run(attempts: 5, delay: 1, expect: vpn_ip) { SplitTunnel.check_ip(@only_vpn_query) }
                expect(only_vpn_result).to eq(vpn_ip)

                # An app added to "Bypass VPN" should never connect to VPN
                bypass_vpn_result = Retriable.run(attempts: 5, delay: 1, expect: proc {|result| result != vpn_ip}) { SplitTunnel.check_ip(@bypass_vpn_query) }
                expect(bypass_vpn_result).not_to eq(vpn_ip)
            end
            it "When disconnected from VPN", :aggregate_failures do
                SplitTunnel.setup_split_tunnel_default_route(defaultTunnelAction)
                SplitTunnel.set_app_split_tunnel_rule(@os)
    
                # An app not specified in split tunnel rules should be able to connect to the internet when disconnected from VPN
                control_result =  Retriable.run(attempts: 5, delay: 1, expect: true) { SplitTunnel.can_connect(@control_query) }
                expect(control_result).to be_truthy
    
                # An app added to "Bypass VPN" should NOT be able to connect to the internet when disconnected from VPN
                only_vpn_result = Retriable.run(attempts: 5, delay: 1, expect: false) { SplitTunnel.can_connect(@only_vpn_query) }
                expect(only_vpn_result).to be_falsey

                # An app added to use "Only VPN" should be able to connect to the internet when disconnected from VPN
                bypass_vpn_result = Retriable.run(attempts: 5, delay: 1, expect: true) { SplitTunnel.can_connect(@bypass_vpn_query) }
                expect(bypass_vpn_result).to be_truthy 
            end
        end
    end
end
