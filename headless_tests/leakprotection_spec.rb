require_relative 'src/piactl'
require_relative 'src/leakchecker'
require_relative 'src/retry'
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
        @leak_checker = LeakChecker.new
        @test_methods = Retriable.run(attempts: 2, delay: 1, expect: proc {|methods| methods.length > 0}) {
            @leak_checker.available_methods
        }
        expect(@test_methods.length > 0).to be_truthy, "No leak testing methods found!"
    end


    def expect_leak
        @test_methods.each do |test_method|
            leak_found = Retriable.run(attempts: 10, delay: 0.75, expect: true) { @leak_checker.leaks? test_method }
            expect(leak_found).to be_truthy, "#{test_method} should have found a leak"
        end
    end

    def expect_no_leak
        @test_methods.each do |test_method|
            leak_found = Retriable.run(attempts: 10, delay: 0.75, expect: false) { @leak_checker.leaks? test_method }
            expect(leak_found).to be_falsey, "#{test_method} should not have found a leak"
        end
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
            [:auto, :on].each do |killswitch_state|
                # If this failed the system's connection could become unusable
                it "Disables killswitch=#{killswitch_state} when logging out" do
                    set_killswitch_and_state "#{killswitch_state}", :connect
                    PiaCtl.logout
                    expect_leak
                end
            end
            after(:each) do	
                PiaCtl.login(ENV['PIA_CREDENTIALS_FILE'])	
            end
        end
    else
        puts "Set PIA_CREDENTIALS_FILE to test for logout issues"
    end
end