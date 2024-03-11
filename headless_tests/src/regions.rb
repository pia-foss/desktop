

class Regions
    def self.get_subset_of_regions
        max_regions = 10

        all_regions = get_all_regions

        if all_regions.length < max_regions
            puts "List of regions is not returning. Dumping the contents of the list:"
            puts all_regions
            exit 1
        end

        # Shuffle the regions list so we try a different subset every time
        all_regions.sample(max_regions)
    end

    def self.check_region_connectivity(region)
        # Set the region to the specified value and connect the vpn.
        # Returns the Ip address obtained for the region.
        PiaCtl.set("region", region)
        PiaCtl.connect
        ip = PiaCtl.get_vpn_ip
        puts "#{region} connected at IP: #{ip}"

        PiaCtl.disconnect
        ip
    end

    def self.get_all_regions
        available_regions = PiaCtl.get("regions").split("\n")
        #Remove the auto region as it could repeat an Ip address, failing the test.
        available_regions.delete("auto")
        available_regions
    end
end
