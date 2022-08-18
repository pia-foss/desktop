require_relative 'spec_helper'
require_relative '../model/build'

#
# The Build class sets up a directory for building artifacts.
#
# Build determines the overall build directory (based on configuration, etc.)
# and creates a per-component build directory for this component.
RSpec.describe Build do
    let(:dir_name) { "foo" }
    let(:build) { Build.new(dir_name, StubModule) }

    describe "component directory" do
        it "ends with provided subdirectory" do
            expect(build.componentDir).to end_with dir_name
        end

        it "starts with out/" do
            expect(build.componentDir).to start_with "out/"
        end

        it "contains the platform in the path" do
            expect(build.componentDir).to match(/#{Build::BuildPlatform}/)
        end

        # We stub out the rake BuildDSL#directory method - otherwise a real
        # directory will be created on the file system which we don't want in tests.
        module StubModule
            def directory(*); end
        end

        # Verify the 'directory' method is invoked - when the Rake DSL is mixed in
        # (rather than the stub we pass in the constructor) it'll actually create
        # the component directory.
        it "creates the component directory" do
            expect_any_instance_of(StubModule).to receive(:directory)
            build
        end
    end

    describe "artifacts" do
        let(:artifact_name) { "bar" }
        let(:artifact_path) { build.artifact(artifact_name) }

        it "nests the artifact under the componentDir" do
            expect(artifact_path).to start_with build.componentDir
            expect(artifact_path).to end_with artifact_name
        end
    end
end

