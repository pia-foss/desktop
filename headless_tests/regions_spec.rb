require_relative 'src/piactl.rb'
require_relative 'src/regions.rb'


describe "Conecting to different regions" do
    ips= []
    describe "when trying to switch to a different region" do
        it "successfully connects to the new region" do
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
end
