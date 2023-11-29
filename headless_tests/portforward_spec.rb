require_relative 'src/piactl'
require_relative 'src/loadedlibchecker'
require_relative 'src/nethelp'
require_relative 'src/awslambda'
require_relative 'src/retry'


describe "Port forwarding" do
    describe "when not requested" do
        it "doesn't get assigned" do
            PiaCtl.set("requestportforward", "false")
            monitor = PiaCtlMonitor.new "portforward"
            PiaCtl.connect
            expect(monitor.peek).to eq "Inactive"
            expect(monitor.expect_change 5).to be_falsey, "Port forward value should not have changed"
            monitor.stop
        end
    end
    
    describe "when requested" do
        it "gets assigned" do
            PiaCtl.set("requestportforward", "true")
            monitor = PiaCtlMonitor.new "portforward"
            expect(monitor.peek).to eq nil
            PiaCtl.connect
            expect(monitor.expect_match /\d+/).to be_truthy, "Port forward value should have been set"
            monitor.stop
        end

        it "can be accessed from the internet" do
            skip "No AWS Credentials found to invoke message sender" unless NetHelp.can_send_message_externally?
            PiaCtl.set("requestportforward", "true")
            PiaCtl.connect

            # Listen on the port and send a message to it from the AWS lambda
            port = PiaCtl.get_forwarded_port
            server = NetHelp::SimpleMessageReceiver.new(port)
            # It can take time for the full forwarding to be ready, so we will try to send messages a few times.
            message_received = Retriable.run(attempts: 3, delay: 3, expect: true) {
                NetHelp.send_message_externally(PiaCtl.get_vpn_ip, port, "portforwardtest")
                sleep 0.1
                server.message_received?
            }
            if message_received
                server.cleanup
                expect(server.message).to eq "portforwardtest"
            else
                server.kill_and_cleanup
                fail "No message received through the forwarded port"
            end
        end
    end
end
