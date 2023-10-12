require_relative 'checksystem'

# Helper class to manipulate PIA state through the piactl binary.
class PiaCtl
    # Set path to piactl from env if available.
    # Otherwise use whichever can be found in path
    PIACTL = ENV['PIACTL'] || 'piactl'
    puts "Using PIACTL=#{PIACTL}"

    def self.run(*args)
        IO.popen([PIACTL, *args].join(' '), "r+")
    end
  
    def self.run_and_wait(*args)
      io = self.run(args)
      _pid, status = Process.waitpid2(io.pid)
      return status.exitstatus
    end
  
    # Connect to the configured region
    def self.connect
        connection_monitor = PiaCtlMonitor.new("connectionstate")
        exit_code = run_and_wait(['connect'])
        if exit_code != 0
            return exit_code
        end
        connection_monitor.expect "Connected"
        connection_monitor.stop
    end

    def self.disconnect
        connection_monitor = PiaCtlMonitor.new("connectionstate")
        exit_code = run_and_wait(['disconnect'])
        if exit_code != 0
            return exit_code
        end
        connection_monitor.expect "Disconnected"
        connection_monitor.stop
    end

    def self.login(credentials_file)
        exit_code = run_and_wait(['login', credentials_file, '-t', '20'])
    end

    def self.logout
        exit_code = run_and_wait(['logout', '-t', '20'])
    end

    def self.get(type)
       get = PiaCtl.run(['get', type])
       result = get.read
       get.close
       return result
    end

    # Set the value of a setting
    def self.set(setting, value)
        PiaCtl.run_and_wait(['set', setting, value])
    end

    def self.set_unstable(setting, value)
        PiaCtl.run_and_wait(['--unstable', 'applysettings', "{\\\"#{setting}\\\":\\\"#{value}\\\"}"])
    end

    def self.get_unstable(setting)
        io = PiaCtl.run(['--unstable', 'dump', 'daemon-settings'])
        json_output = ""
        while (line = io.gets)
            json_output << line
        end
        settings = JSON.parse(json_output)
        settings[setting]
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
    def expect(new_value)
        while @queue.pop != new_value; end
    end
  
    # Retrieve the last monitored value
    def peek
      @last_line
    end
  
    def stop
        # We cannot kill directly for windows, so we call taskkill
        case check_system
        when :windows
            system "taskkill /F /pid #{@io.pid}", :out => File::NULL
        else
            Process.kill("TERM", @io.pid)
            Process.wait(@io.pid)
        end
        @stdout_thread.join
    end
end
