require_relative 'src/piactl'
require_relative 'src/loadedlibchecker'
require_relative 'src/systemutil'

if SystemUtil.linux?
    describe "Dependency loading" do
        describe "pia-daemon" do
            it "doesn't load system libraries" do
                incorrectly_loaded_libs = LoadedLibChecker.find_incorrect_libs "pia-daemon"
                expect(incorrectly_loaded_libs).to be_empty, "the libraries #{incorrectly_loaded_libs} should have been loaded from /opt/piavpn"
            end
        end
        
        describe "pia-client" do
            it "doesn't load system libraries" do
                begin
                    incorrectly_loaded_libs = LoadedLibChecker.find_incorrect_libs "pia-client"
                    expect(incorrectly_loaded_libs).to be_empty, "the libraries #{incorrectly_loaded_libs} should have been loaded from /opt/piavpn"
                rescue ProcessNotFound => e
                    # pia-client is optional because we cannot run it in CI
                    skip e.message
                end
            end
        end
        
        describe "pia-unbound" do
            it "doesn't load system libraries" do
                # Launch pia-unbound
                PiaCtl.set_unstable("overrideDNS", "local")
                PiaCtl.connect
                incorrectly_loaded_libs = LoadedLibChecker.find_incorrect_libs "pia-unbound"
                PiaCtl.disconnect
                PiaCtl.set_unstable("overrideDNS", "pia")

                expect(incorrectly_loaded_libs).to be_empty, "the libraries #{incorrectly_loaded_libs} should have been loaded from /opt/piavpn"
            end
        end
    end
end