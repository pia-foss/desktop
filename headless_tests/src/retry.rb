class Retriable

    # Sets up to run the given block `attempts` times if it fails due to exceptions or 
    # because the value is not as expected.
    # As soon as the value returned in the block matches expectations 
    # it will return it. Otherwise, when it runs out of attempts it returns
    # the incorrect value from the block or raises the last exception it raised.
    def self.with_retry(attempts:, delay: nil, expect: proc(&:itself), &retry_block)
        new(attempts, delay, expect, &retry_block)
    end

    # Sets up and runs the block with retries
    def self.run(attempts:, delay: nil, expect: proc(&:itself), &retry_block)
        with_retry(attempts: attempts, delay: delay, expect: expect, &retry_block).start
    end

    def initialize(attempts, delay, expect, &retry_block)
        raise "block needed" unless block_given?
    
        @retry_block = retry_block
        @attempts = attempts
        @delay = delay
        @failure_block = @trace_block = nil
        @expected = expect
    end

    # what to do on failure after all attempts (optional)
    def failure(&failure_block)
        @failure_block = failure_block
        self
    end

    # for tracing/logging between attempts (optional)
    def trace(&trace_block)
        @trace_block = trace_block
        self
    end

    # an optional way to setup the expected outcome
    def expect(expected_output)
        @expected = expected_output
        self
    end

    def start
        result = nil
        last_exception = nil
        (1..@attempts).each do |count|
            result = nil
            begin
                result = @retry_block.call
                @trace_block.call(count) if @trace_block
                last_exception = nil
            rescue => exception
                # Capture exceptions
                last_exception = exception
            end
            
            if @expected.is_a? Proc
                return result if @expected.call(result)
            else
                return result if result == @expected
            end

            # Only sleep if we're not already on the final attempt
            if count < @attempts
                sleep(@delay) if @delay
            elsif result
                return result
            end
        end
        
        @failure_block.call(result, last_exception, @attempts) if @failure_block

        raise last_exception if last_exception != nil

        result
    end
end