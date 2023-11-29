require_relative 'src/piactl'
require_relative 'src/splittunnel'
require_relative 'src/systemutil'


if SystemUtil.macos?
    return
end
describe "Split tunnel" do
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
                use_vpn_curl_result = SplitTunnel.curl_for_ip(@use_vpn_ip)
                # Depends on whether split tunnel is set to bypass/use VPN by default
                if defaultTunnelAction == :useVpn
                    expect(use_vpn_curl_result).to eq(@vpn_ip)
                elsif defaultTunnelAction == :bypassVpn
                    expect(use_vpn_curl_result).not_to eq(@vpn_ip)
                end

                bypass_vpn_curl_result = SplitTunnel.curl_for_ip(@bypass_vpn_ip)
                # Should always bypass regardless of default setting
                expect(bypass_vpn_curl_result).not_to eq(@vpn_ip)
            end
        end
    end
end
