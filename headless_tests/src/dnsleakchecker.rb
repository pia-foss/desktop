require 'json'
require 'securerandom'
require 'net/http'
require 'open3'
require_relative 'systemutil'

# Uses bash.ws API to test for dns leaks
class DNSLeakChecker
    API_DOMAIN = 'bash.ws'

    # bash.ws works by first pinging the domain with a specific id and then
    # querying the API by the same id. 
    # The id is a random number and could potentially clash with another user of the API.
    def self.dns_leaks?
        id = SecureRandom.random_number(9999999)
        # Send all requests in parallel, as they will all fail and timeout

        command = SystemUtil.local_traffic_test_command

        10.times.map { |i| Thread.new { system("#{command} #{i+1}.#{id}.#{API_DOMAIN}", out: File::NULL, err: File::NULL) } }.each(&:join) 
        
        output, err_out, status = Open3.capture3("curl https://#{API_DOMAIN}/dnsleak/test/#{id}?json --connect-timeout 10")
        raise "Could not reach DNS leaks API #{err_out}" if status != 0

        result_json = JSON.parse(output)
        conclusion = result_json.select { |r| r["type"] == "conclusion" }.first["ip"]
        conclusion == "DNS may be leaking."
    end
end
