# Refer to https://rubydoc.info/gems/rspec-core/RSpec/Core/Configuration
require_relative '../src/piactl'

RSpec.configure do |config|
  # rspec-expectations config goes here.
  config.expect_with :rspec do |expectations|
    # Defaults to `true` in RSpec 4. It makes the `description` and `failure_message`
    # of custom matchers include text for helper methods defined using `chain`
    expectations.include_chain_clauses_in_custom_matcher_descriptions = true
  end

  # rspec-mocks config goes here.
  config.mock_with :rspec do |mocks|
    # Prevents you from mocking or stubbing a method that does not exist on a real
    # object. This is generally recommended, and will default to `true` in RSpec 4.
    mocks.verify_partial_doubles = true
  end

  # Defaults to `:apply_to_host_groups` in RSpec 4. It causes shared context metadata 
  # to be inherited by the metadata hash of host groups and examples, rather than
  # triggering implicit auto-inclusion in groups with matching metadata.
  config.shared_context_metadata_behavior = :apply_to_host_groups

  # Run specs in random order to surface order dependencies. You can fix 
  # the order by providing the seed, which is printed after each run.
  #     --seed 1234
  config.order = :random

  # Allows the suite to be run specifying the protocol at runtime, or else defaults
  # to selecting at random. Example usage:
  # PROTOCOL=wireguard rspec .
  protocol_choice = ENV['PROTOCOL'] || "random"
 
  # Don't run tests that rely on the other protocol when one is specified at runtime
  config.filter_run_excluding :openVPNOnly => true if protocol_choice == "wireguard"
  config.filter_run_excluding :wireguardOnly => true if protocol_choice == "openvpn"

  config.before(:suite) do
    # Ensure the app is in a default state before each test is run
    set_up_state protocol_choice
  end

  config.after(:each) do
    # Clean up app state after the suite is run.
    set_up_state protocol_choice
  end

  def set_up_state(protocol)
    PiaCtl.disconnect
    PiaCtl.resetsettings
    PiaCtl.set("region", "auto")
    protocol = PiaCtl::Protocols.sample if protocol == "random"    
    PiaCtl.set("protocol", protocol)
  end
end
