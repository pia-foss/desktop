require "simplecov"

SimpleCov.start do
    # Remove our spec files from result.
    # we don't want to measure coverage on the specs themselves
    add_filter /_spec/
end

require "pry"
require_relative "../buildsystem"

