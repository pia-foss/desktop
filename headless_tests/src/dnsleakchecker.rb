require 'json'
require 'securerandom'
require 'net/http'

# Uses bash.ws API to test for dns leaks
class DNSLeakChecker
    API_DOMAIN = 'bash.ws'

    # bash.ws works by first pinging the domain with a specific id and then
    # querying the API by the same id. 
    # The id is a random number and could potentially clash with another user of the API.
    def self.dns_leaks?
        id = SecureRandom.random_number(9999999)
        # Send all ping requests in parallel, as they will all fail and timeout
        10.times.map { |i| Thread.new { system("ping", "-c", "1", "#{i+1}.#{id}.#{API_DOMAIN}", out: File::NULL, err: File::NULL) } }.each(&:join) 
        
        response = Net::HTTP.get_response URI("https://#{API_DOMAIN}/dnsleak/test/#{id}?json") 
        result_json = JSON.parse(response.body)
        conclusion = result_json.select { |r| r["type"] == "conclusion" }.first["ip"]
        conclusion == "DNS may be leaking."
    end
end
