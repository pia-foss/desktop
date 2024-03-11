require 'etc'

class ProcessNotFound < StandardError
end

class SystemUtil
    def self.loaded_libs(process_name)
        case os
        when :linux
            process_pid = pid process_name
            if !process_pid
                raise ProcessNotFound.new "No process #{process_name} found"
            end

            # Include timeout in case password prompt is not noticed when running locally. 
            pldd_cmd = "timeout 30s sudo pldd #{process_pid}"
            io = IO.popen(pldd_cmd, "r+")
            loaded_libs = []
            while (line = io.gets)
                # Discard lines that don't start with / as they are not libs
                loaded_libs.append(line) if line.start_with? '/'
            end
            _pid, status = Process.waitpid2(io.pid)
            if status != 0
                raise "Failed to run cmd #{pldd_cmd}"
            end
            loaded_libs
        else
            raise "loaded libs not implemented for #{SystemUtil.os}"
        end
    end
    
    def self.pid(process_name)
        pids = `pgrep #{process_name}`
        pids.split()[0]
    end
    
    def self.os
        case Etc.uname[:sysname]
        when "Windows_NT" 
            :windows 
        when "Linux" 
            :linux 
        when "Darwin" 
            :macos 
        end
    end
    
    def self.windows?
        os == :windows
    end

    def self.linux?
        os == :linux
    end

    def self.macos?
        os == :macos
    end

    def self.CI?
        ENV["GITHUB_CI"] != nil
    end

    def self.local_traffic_test_command
        if macos?
            # pings are blocked in Github-hosted runners, so we use nslookup.
            "nslookup google.com"
        elsif Command.execute("which traceroute")
            # This will be the default for Linux in CI, and for any local Linux machine with traceroute installed. 
            # Ping is blocked in GHA hosted runners, and nslookup gives false failures in ubuntu.
            "traceroute -m 1"
        else
            # We use self-hosted Windows runners in CI so ping is not blocked.
            "ping -n 1"
        end
    end
end

