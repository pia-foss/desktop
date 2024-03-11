require 'timeout'
require 'json'
require_relative 'systemutil'

# Helper class to manipulate PIA state through the piactl binary.
class PiaCtl
    # Set path to piactl from env if available.
    # Otherwise use whichever can be found in path
    PIACTL = ENV['PIACTL'] || 'piactl'

    Protocols = ["openvpn", "wireguard"]
    
    def self.run(args)
        IO.popen [PIACTL, *args], "r+"
    end
  
    def self.run_and_wait(args)
        io = self.run(args)
        _pid, status = Process.waitpid2(io.pid)
        io.close
        status.exitstatus
    end
  
    # Connect to the configured region
    def self.connect
        connection_monitor = PiaCtlMonitor.new("connectionstate")
        exit_code = run_and_wait(['connect'])
        if exit_code != 0
            raise "Failed to connect"
        end

        connection_monitor.expect "Connected"
        connection_monitor.stop
    end

    def self.disconnect
        connection_monitor = PiaCtlMonitor.new("connectionstate")
        exit_code = run_and_wait(['disconnect'])
        connection_monitor.expect "Disconnected"
        connection_monitor.stop
    end

    def self.login(credentials_file)
        exit_code = run_and_wait(['login', credentials_file, '-t', '60'])
        if exit_code != 0
            raise "Login failed!"
        end
    end

    def self.logout
        exit_code = run_and_wait(['logout', '-t', '60'])
        if exit_code != 0
            raise "Logout failed!"
        end
    end

    def self.get(type)
        get = PiaCtl.run(['get', type])
        result = get.read
        get.close
        result = result.chomp if result
        result
    end

    def self.set(setting, value)
        PiaCtl.run_and_wait(['set', setting, value])
    end

    def self.set_unstable(setting, value)
        settings = {setting => value}.to_json
        PiaCtl.run_and_wait(['--unstable', 'applysettings', settings])
    end

    def self.get_unstable(setting)
        io = PiaCtl.run(['--unstable', 'dump', 'daemon-settings'])
        json_output = ""
        while (line = io.gets)
            json_output << line
        end
        settings = JSON.parse(json_output)
        io.close
        settings[setting]
    end

    def self.get_vpn_ip
        monitor = PiaCtlMonitor.new("vpnip")
        monitor.expect_match /\d+\.\d+\.\d+\.\d+/
        vpn_ip = monitor.peek
        monitor.stop
        vpn_ip
    end

    def self.get_forwarded_port
        monitor = PiaCtlMonitor.new "portforward"
        monitor.expect_match /\d+/
        port = monitor.peek.to_i
        monitor.stop
        port
    end

    def self.resetsettings
        PiaCtl.run_and_wait(['resetsettings'])
    end
end

# Helper to monitor a setting with piactl
class PiaCtlMonitor
    # Initializes a process for monitoring.
    # The output of the process will be stored from a separate thread
    # in a queue to wait for expected values with the expect method.
    def initialize(setting)
      @io = PiaCtl.run(["monitor", setting])
      @queue = Queue.new
      @stdout_thread = setup_stdout_thread
    end
  
    def setup_stdout_thread
        Thread.new do
            while (line = @io.gets)
                if line != @last_line
                    @last_line = line.chomp
                    @queue << @last_line
                end
            end
        end
    end

    # Wait for the monitored value to equal new_value
    def expect(new_value, timeout=20)
        begin
            Timeout.timeout(timeout) do
                loop do
                    current_value = @queue.pop
                    break if current_value == new_value
                end
            end
        rescue Timeout::Error => e
            self.stop
            puts "Timed out!"
            raise
        end
        true
    end

    def expect_match(regex, timeout=20)
        Timeout.timeout(timeout, Timeout::Error) do
            while @queue.pop !~ regex
            end
        rescue Timeout::Error => e
            raise Timeout::Error
        end
        true
    end

    # Returns true if the value changes before the timeout runs out.
    # Call it *before* performing an action that you expect should (or shouldn't) change a setting.
    def expect_change(timeout=20)
        # Make sure the queue is empty
        @queue.clear()
        Timeout.timeout(timeout, Timeout::Error) do
            @queue.pop
            true
        rescue Timeout::Error => e
            false
        end
    end
    
    # Retrieve the last monitored value
    def peek
      @last_line
    end
  
    def stop
        # We cannot kill directly for windows, so we call taskkill
        if SystemUtil.windows?
            system "taskkill /F /pid #{@io.pid}", :out => File::NULL
        else
            Process.kill("TERM", @io.pid)
            Process.wait(@io.pid)
        end
        @stdout_thread.join
    end
end
