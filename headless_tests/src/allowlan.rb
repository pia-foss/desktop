require_relative 'systemutil'
require_relative 'command'

class AllowLan
    def self.test_local_traffic(local_ip)
        command_to_run = SystemUtil.local_traffic_test_command
        Command.execute("#{command_to_run} #{local_ip}")
    end
end
