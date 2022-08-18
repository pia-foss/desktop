require_relative 'model/build.rb'

# Generate Info.plist content on Mac.  Used by Executable (for console
# executables on Mac) and by PiaMacOS.
class MacInfoPlist
    # A default MacInfoPlist contains content common to all bundles generated by
    # PIA Desktop
    def initialize(bundleName, executable, identifier)
        @plistData = {}
        @plistData['BuildMachineOSBuild'] = `sw_vers -buildVersion`.strip
        @plistData['CFBundleDevelopmentRegion'] = 'en'
        @plistData['CFBundleDisplayName'] = bundleName
        @plistData['CFBundleExecutable'] = executable
        @plistData['CFBundleIdentifier'] = identifier
        @plistData['CFBundleInfoDictionaryVersion'] = '6.0'
        @plistData['CFBundleName'] = bundleName
        @plistData['CFBundleShortVersionString'] = Build::VersionBase
        @plistData['CFBundleVersion'] = Build::VersionBase
    end

    # Add default Info.plist content for any macOS bundle (app or framework)
    def macos()
        @plistData['LSMinimumSystemVersion'] = "#{Build::MacosVersionMajor}.#{Build::MacosVersionMinor}"
    end

    # Add default Info.plist content for any iOS bundle
    def ios()
        @plistData['CFBundleSupportedPlatforms'] = ['iPhoneOS']
        @plistData['MinimumOSVersion'] = '9.0' # TODO - get from Build
        @plistData['UIDeviceFamily'] = [1, 2] # TODO - what is this?
    end

    # Add default Info.plist content for any iOS Simulator bundle
    def iossim()
        @plistData['CFBundleSupportedPlatforms'] = ['iPhoneSimulator']
        @plistData['MinimumOSVersion'] = '9.0' # TODO - get from Build
        @plistData['UIDeviceFamily'] = [1, 2] # TODO - what is this?
    end

    # Add default Info.plist content for macOS application
    # (includes defaultBundle and defaultMacos - we don't ship any app bundles
    # for platforms other than macOS from PIA Desktop)
    def macosApp()
        macos
        @plistData['CFBundlePackageType'] = 'APPL'
        @plistData['CFBundleSignature'] = '????'
        @plistData['NSPrincipalClass'] = 'NSApplication'
        self
    end

    # Add default Info.plist content for a framework (either macOS or iOS)
    def framework()
        @plistData['CFBundlePackageType'] = 'FMWK'
        self
    end

    # Add default Info.plist content for the target platform (macos, ios, or
    # iossim)
    def targetPlatform()
        macos if Build::macos?
        ios if Build::Platform == :ios
        iossim if Build::Platform == :iossim
        self
    end

    def self.renderPlistValue(content, value, linePrefix)
        if(value.is_a?(String))
            # The value isn't escaped at all, currently no values used in
            # property lists require escaping
            content << "#{linePrefix}<string>#{value}</string>\n"
        elsif(value.is_a?(Array))
            content << "#{linePrefix}<array>\n"
            innerPrefix = linePrefix + "\t"
            value.each { |e| renderPlistValue(content, e, innerPrefix) }
            content << "#{linePrefix}</array>\n"
        elsif(value.is_a?(Hash))
            content << "#{linePrefix}<dict>\n"
            innerPrefix = linePrefix + "\t"
            value.each do |k, v|
                content << "#{innerPrefix}<key>#{k}</key>\n"
                renderPlistValue(content, v, innerPrefix)
            end
            content << "#{linePrefix}</dict>\n"
        else
            content << "#{linePrefix}<#{!!value}/>\n"
        end
    end

    # Render a plist as XML from a Hash containing array, string, and/or boolean
    # values
    def renderPlistXml()
        content = ''

        content << '<?xml version="1.0" encoding="UTF-8"?>' << "\n"
        content << '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' << "\n"
        content << '<plist version="1.0">' << "\n"
        MacInfoPlist::renderPlistValue(content, @plistData, '')
        content << '</plist>' << "\n"
        content
    end

    # Get any value
    def get(name)
        @plistData[name]
    end

    # Set a custom value
    def set(name, value)
        @plistData[name] = value
        self
    end

    # Shortcut to render a macOS App bundle plist with no customization
    def self.renderMacosAppPlist(bundleName, executable, identifier)
        MacInfoPlist.new(bundleName, executable, identifier).renderPlistXml
    end
end
