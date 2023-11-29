# Headless tests

This is a collection of tests for PIA Desktop using `piactl` as the client.


## Additional requirements

Some tests will not be possible to run on a clean system. 
Such tests should simply be disabled without errors, and you will need to configure the system if you want to run them.

### Port Forwarding
It is not possible to test port forwarding from a single machine. 
Instead, we use a simple lambda function on AWS that will send a message to a specified address and port.

To call it, we need to set the following environment variables:

* PIA_AWS_SEND_MESSAGE_LAMBDA -> name of the lambda function
* PIA_AWS_LAMBDA_REGION -> lambda region
* PIA_AWS_LAMBDA_KEY_ID -> key id with permissions to invoke the lambda
* PIA_AWS_LAMBDA_ACCESS_KEY -> secret key for the key id
