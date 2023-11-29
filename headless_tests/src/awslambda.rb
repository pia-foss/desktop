require 'aws-sdk-lambda'
require 'json'

class AWSLambda
    def initialize
        # Set up AWS credentials
        Aws.config.update(AWSLambda.get_credentials)

        @client = Aws::Lambda::Client.new
    end

    def invoke(function_name, payload)
        resp = @client.invoke({
            function_name: function_name,
            invocation_type: 'RequestResponse',
            payload: payload.to_json
        })
        resp.payload
    end

    def self.get_credentials
        {
            region: ENV['PIA_AWS_LAMBDA_REGION'],
            access_key_id: ENV['PIA_AWS_LAMBDA_KEY_ID'],
            secret_access_key: ENV['PIA_AWS_LAMBDA_ACCESS_KEY']
        }
    end

    def self.has_credentials?
        ENV['PIA_AWS_LAMBDA_REGION'] && ENV['PIA_AWS_LAMBDA_KEY_ID'] && ENV['PIA_AWS_LAMBDA_ACCESS_KEY'] 
    end
end