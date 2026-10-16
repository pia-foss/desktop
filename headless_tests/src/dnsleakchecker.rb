require 'json'
require 'securerandom'
require 'net/http'
require 'open3'
require_relative 'systemutil'
require_relative 'retry'

class DNSLeakChecker
    # The leak tests work by first pinging the domain with a specific id and then
    # querying the API by the same id.
    # The id is a random number and could potentially clash with another user of the API.
    def self.dns_leaks?
        session_id = SecureRandom.hex(8) # bash.ws rejects ids longer than 16 chars
        begin
            leak_detected_bash_ws?(session_id)
        rescue
            # Try a different leak testing tool if the first one fails
            leak_detected_dnsleaktest_org?(session_id)
        end
    end

    def self.leak_detected_bash_ws?(session_id)
        test_path = "#{session_id}.bash.ws"
        check_path = "https://bash.ws/dnsleak/test/#{session_id}?json"

        output = check_requests(test_path, check_path)

        result_json = JSON.parse(output)
        conclusion = result_json.select { |r| r["type"] == "conclusion" }.first["ip"]
        conclusion == "DNS may be leaking."
    end

    def self.leak_detected_dnsleaktest_org?(session_id)
        test_path = "#{session_id}.test.dnsleaktest.org"
        check_path = "https://dnsleaktest.org/api/dnsecho/check/#{session_id}"
        # Get information about the current IP address
        out = Retriable.run(attempts: 2, delay: 2) {
            o, e, s = Open3.capture3("curl https://dnsleaktest.org/api/ip --connect-timeout 10")
            raise "Could not reach DNS leaks API #{e}" unless s.success?
            o
        }
        ip_info = JSON.parse(out).slice("cc", "asnOrg", "isp", "cityName")

        output = check_requests(test_path, check_path)

        result_list = JSON.parse(output)["data"]
        # If any of the results don't match, we have a leak
        result_list.any? { |result| result.slice("cc", "asnOrg", "isp", "cityName") != ip_info }
    end

    def self.check_requests(test_path, check_path)
        # Send all requests in parallel, as they will all fail and timeout
        10.times.map { |i| Thread.new { system("curl #{i+1}.#{test_path}", out: File::NULL, err: File::NULL) } }.each(&:join)
        output, err_out, status = Retriable.run(attempts: 2, delay: 2) {
            o, e, s = Open3.capture3("curl #{check_path} --connect-timeout 10")
            raise if ((o.include? '"retcode":-100') || (o.include? 'error'))
            [o, e, s]
        }
        raise "Could not reach DNS leaks API #{err_out}" unless status.success?
        output
    end
end
