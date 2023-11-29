require_relative 'src/piactl.rb'
require_relative 'src/allowlan.rb'
require_relative 'src/nethelp.rb'
require_relative 'src/dnsleakchecker.rb'
require_relative 'src/retry.rb'
require_relative 'src/systemutil'

# "" is actually a valid value for overrideDNS
# and represents the "Use existing DNS" option
test_cases = [
    {dns: "pia", expected: ["10.0.0.243"], allowed_to_leak: false},
    {dns: "local", expected: ["127.80.73.65"], allowed_to_leak: false}, # Fun fact, this is 127.P.I.A
    {dns: "hdns", expected: ["103.196.38.38"], allowed_to_leak: true},
    {dns: ["1.1.1.1", "1.1.1.2"], expected: ["1.1.1.1", "1.1.1.2"], allowed_to_leak: true},
    {dns: ["127.0.0.1", "1.1.1.2"], expected: ["127.0.0.1", "1.1.1.2"], allowed_to_leak: true},
]
# Local DNS does not play nice with our windows CI runner. Not worth to keep fighting it now, skip.
if !(ENV["GITHUB_CI"] && SystemUtil.windows?)
    test_cases << {dns: "", expected: :default_name_server, allowed_to_leak: true}
end

test_cases.each do |test_case|
    describe "With DNS overriden to \"#{test_case[:dns]}\"" do
        # This is done only once, before any test is run
        before(:each) do
            # Start from a disconnected state
            PiaCtl.set_unstable("overrideDNS", test_case[:dns])
            @default_name_server = NetHelp.get_default_nameserver
            test_case[:expected] = [@default_name_server] if test_case[:expected] == :default_name_server
        end

        describe "when disconnected from the VPN" do
            it "the default nameserver is unchanged from the system default" do
                current_nameserver = NetHelp.get_default_nameserver
                expect(current_nameserver).to eq @default_name_server
            end
        end

        describe "when connected to the VPN" do
            before(:each) do
                Retriable.run(attempts: 2, delay: 2) { PiaCtl.connect }
            end

            it "the default nameserver is any of #{test_case[:expected]}" do
                current_nameserver = NetHelp.get_default_nameserver
                expect(test_case[:expected].include? current_nameserver).to be_truthy, "Unexpected nameserver #{current_nameserver}"
            end

            if test_case[:allowed_to_leak]
                it "DNS requests leak" do
                    leak_found = Retriable.run(attempts: 3, delay: 2, expect: true) { DNSLeakChecker.dns_leaks? }
                    if !leak_found
                        skip "No leak was found, we can skip this test"
                    end
                    expect(leak_found).to be_truthy
                end
            else
                it "DNS requests do not leak" do
                    leak_found = Retriable.run(attempts: 8, delay: 3, expect: false) { DNSLeakChecker.dns_leaks? }
                    expect(leak_found).to be_falsey, "Unexpected DNS leak detected"
                end
            end
        end
    end
end
