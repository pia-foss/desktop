require_relative 'src/piactl.rb'
require_relative 'src/regions.rb'
require_relative 'src/nethelp.rb'


describe "Conecting to a subset of different regions" do
    ips= []
    it "successfully connects to each new region" do
        # As our tests aim to test the code, not the backend, some number of 
        # connections failures are acceptable. This can be due to temporarily 
        # bad regions. So this test tries to connect 10 times to different regions,  
        # and as long as over half of the attempts suddeeds, we pass the test.
        failures = 0
        for region in Regions.get_subset_of_regions do
            begin
                region_ip = Regions.check_region_connectivity(region)
                expect(region_ip != "Unknown").to be_truthy, "Can not retrieve IP "
                expect(ips).not_to include region_ip, "IP is not unique"

                ips.push(region_ip)
                puts "Region #{region} OK"
            rescue Timeout::Error => e
                warn "#{region} failed to connect"
                failures = failures + 1
                next
            end
        end
        expect(failures <= 5).to be_truthy, "More than half the regions failed to connect."
    end
end

#This test is part of our checks for overall service health. 
# It is not meant to be run by default in a standard run.
describe "Connecting to all available regions", :servicecheck=>true do
    ips= []
    Regions.get_all_regions.each do |region|
        it "Can succesfully connect to #{region}", :aggregate_failures do
            PiaCtl.set("region", region)
            PiaCtl.connect
            region_ip = PiaCtl.get_vpn_ip

            expect(NetHelp.pia_connected?).to be_truthy, "#{region} unable to connect to the internet"

            expect(NetHelp.curl_for_ip).to match(region_ip), "IP from VPN not matching query for IP"

            puts "#{region} connected at IP: #{region_ip}"

            expect(region_ip != "Unknown").to be_truthy, "Can not retrieve IP "
            expect(ips).not_to include region_ip, "IP is not unique"

            ips.push(region_ip)
        end
    end
end
