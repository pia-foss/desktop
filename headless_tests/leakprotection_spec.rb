require_relative 'src/piactl'
require_relative 'src/leakchecker'
require 'uri'
require 'net/http'
require 'socket'
require 'json'

def set_killswitch_and_state(killswitch_value, action)
    PiaCtl.set_unstable("killswitch", killswitch_value)
    case action
    when :connect 
        PiaCtl.connect
    when :disconnect
        PiaCtl.disconnect
    end
end

describe "Leak protection" do
    # We initialize now so target interfaces and local ip addresses in the system won't change
    before(:all) do
        # Start from a disconnected state, otherwise 
        # we can't check for available methods
        set_killswitch_and_state "auto", :disconnect

        @leak_checker = LeakChecker.new
        @test_methods = @leak_checker.available_methods
    end


    def expect_leak
        @test_methods.each do |test_method|
            expect(@leak_checker.leaks_with_retry? test_method, true).to be_truthy, "#{test_method} should have found a leak"
        end
    end

    def expect_no_leak
        @test_methods.each do |test_method|
            expect(@leak_checker.leaks_with_retry? test_method, false).to be_falsey, "#{test_method} should not have found a leak"
        end
    end

    it "There are leak testing methods available" do
        expect(@test_methods.length > 0).to be_truthy, "No leak testing methods found!"
    end

    describe "Testing for leaks when disconnected from the VPN" do
        it "Leaks my IP address when disconnected and killswitch=auto" do
            set_killswitch_and_state "auto", :disconnect
            expect_leak
        end

        it "Leaks my IP address when disconnected and killswitch=off" do
            set_killswitch_and_state "off", :disconnect
            expect_leak
        end

        it "Protects my IP address when disconnected and killswitch=on" do
            set_killswitch_and_state "on", :disconnect
            expect_no_leak
        end
    end

    describe "Testing for leaks when connected to the VPN" do
        it "Protects my IP address when connected and killswitch=auto" do
            set_killswitch_and_state "auto", :connect
            expect_no_leak
        end

        it "Protects my IP address when connected and killswitch=on" do
            set_killswitch_and_state "on", :connect
            expect_no_leak
        end

        it "Leaks my IP address when connected and killswitch=off" do
            set_killswitch_and_state "off", :connect
            expect_leak
        end
    end

    if ENV['PIA_CREDENTIALS_FILE']
        describe "Testing for leaks when logged out of the VPN" do
            # If this failed the system's connection could become unusable
            it "Disables killswitch=auto when logging out" do
                set_killswitch_and_state "auto", :connect
                PiaCtl.logout
                all_leak = false
                @test_methods.each do |test_method|
                    all_leak |= @leak_checker.leaks_with_retry? test_method, true
                end
                # Restore PIA to a usable state
                PiaCtl.login(ENV['PIA_CREDENTIALS_FILE'])
                PiaCtl.set_unstable("killswitch", "auto")
                PiaCtl.disconnect
                expect(all_leak).to be_truthy, "should have found a leak"
            end

            # If this failed the system's connection could become unusable
            it "Disables killswitch=on when logging out" do
                set_killswitch_and_state "on", :connect
                PiaCtl.logout
                all_leak = false
                @test_methods.each do |test_method|
                    all_leak |= @leak_checker.leaks_with_retry? test_method, true
                end
                # Restore PIA to a usable state
                PiaCtl.login(ENV['PIA_CREDENTIALS_FILE'])
                PiaCtl.set_unstable("killswitch", "auto")
                PiaCtl.disconnect
                expect(all_leak).to be_truthy, "should have found a leak"
            end
        end
    else
        puts "Set PIA_CREDENTIALS_FILE to test for logout issues"
    end
end