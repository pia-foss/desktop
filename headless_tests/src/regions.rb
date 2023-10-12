

class Regions
    def self.get_subset_of_regions
        max_regions = 10
        available_regions = PiaCtl.get("regions").split("\n")
        #Remove the auto region as it could repeat an Ip address, failing the test.
        available_regions.delete("auto")

        puts "Found #{available_regions.length} regions."

        if available_regions.length < max_regions
            puts "List of regions is not returning. Dumping the contents of the list:"
            puts available_regions
            exit 1
        end

        # Shuffle the regions list so we try a different subset every time
        return available_regions.sample(10)
    end

    def self.check_region_connectivity(region)
        # Set the region to the specified value and connect the vpn.
        # Returns the Ip address obtained for the region.
        puts "Trying to connect to #{region}..."
        PiaCtl.set("region", region)
        PiaCtl.connect

        ip = "Unknown"
        timeout = Time.now + 60
        while ip == "Unknown" && Time.now < timeout do
            ip = PiaCtl.get("vpnip").chomp
        end
        
        puts "#{region} connected at IP: #{ip}"

        PiaCtl.disconnect
        return ip
    end
end
