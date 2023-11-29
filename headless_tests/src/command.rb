# Run commands multiple times, until the expected outcome
# Both functions return true if exit code is 0 and false otherwise
require 'open3'

class Command
    # Executes command, returns true if the command exits successfully
    def self.execute(command_to_run)
        output, err_out, status = Open3.capture3(command_to_run)
        status.success?
    end

    # Executes the command returns the output
    def self.execute_with_output(command_to_run)
        output, err_out, status = Open3.capture3(command_to_run)
        if !status.success?
            raise "The execution was not successful.\n" +
            "Status = #{status.success?}\n" + output
        end

        output
    end
        
end
