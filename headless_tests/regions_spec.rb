require_relative 'src/piactl.rb'
require_relative 'src/regions.rb'


describe "Conecting to different regions" do
    ips= []

    # This is done only once, before any test is run
    before(:all) do
        # Start from a disconnected state
        PiaCtl.disconnect
    end

    describe "when trying to switch to a different region" do
        Regions.get_subset_of_regions.each do |region|
            it "successfully connects to the new region" do
                region_ip = Regions.check_region_connectivity(region)
            
                # if the ip fails to show, try to connect one more time
                if region_ip == "Unknown"
                    region_ip = Regions.check_region_connectivity(region)
                end

                expect(region_ip != "Unknown").to be_truthy, "Can not retrieve IP "
                expect(ips).not_to include region_ip, "IP is not unique"

                ips.push(region_ip)
                puts "Region #{region} OK"
            end
        end
    end
end
