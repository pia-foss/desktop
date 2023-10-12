# Run commands multiple times, until the expected outcome
# Both functions return true if exit code is 0 and false otherwise

class Command
    # This function executes the command_to_run and
    # returns true if the command exitcode and false otherwise 
    def self.execute?(command_to_run)
        system command_to_run, :out => File::NULL, :err => File::NULL
        exitCode = $?.exitstatus
        if $?.exitstatus == 0
            return true
        else
            return false
        end
    end

    # This function returns true when the command exit code is 
    # the same as expectedResult.
    # This means that a failure has been confirmed by 10 tries.
    # By doing so we remove edge cases of running commands just after
    # the connection status has change to Connected, 
    # which proved to be flaky / not reliable. 
    #
    # expectedResult true is equal to exit code 0.
    def self.execute_with_retry?(command_to_run, expected_result, n: 10)
        puts "Running `#{command_to_run}` up to #{n} times expecting this result: #{expected_result}"
        n.times {
            if execute?(command_to_run) == expected_result
                return expected_result
            end
            sleep(0.75)
        }
        return !expected_result
    end
end
