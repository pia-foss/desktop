# Local build environment variables can be placed in `.buildenv` in the project
# directory to set custom defaults for builds.  (Variables on the rake command
# line still override these.)
#
# For example, if you are working on new branding features and want to make
# release builds of the 'acme' brand by default:
#
# ```
# BRAND=acme
# VARIANT=release
# ```
#
# Or, on macOS, to use a local code signing certificate by default to sign
# installers:
#
# ```
# PIA_CODESIGN_CERT=My certificate's common name
# ```
#
# Or, on Windows, to build x86 by default on an x86_64 host:
# ```
# ARCHITECTURE=x86
# ```

buildEnvContent = File.exist?('.buildenv') ? File.read('.buildenv') : ""

buildEnvContent.each_line do |var|
    # Exclude '#' from the first character, lines starting with '#' are
    # treated as comments (ignored)
    split = var.match(/^([^=#][^=]+)=(.*)$/)
    if(split != nil)
        if(ENV[split[1]])
            # This variable already exists, don't overwrite it.  This is
            # intentional to allow overriding .buildenv from the rake
            # command line, although it does mean that user/system variables
            # cannot be overridden by .buildenv
            puts "ignoring .buildenv #{split[1]}, using value: #{ENV[split[1]]}"
        else
            puts "using .buildenv #{split[1]} value: #{split[2]}"
            ENV[split[1]] = split[2]
        end
    end
end
