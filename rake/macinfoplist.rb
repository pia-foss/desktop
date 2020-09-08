require_relative 'model/build.rb'

# Generate Info.plist content on Mac.  Used by Executable (for console
# executables on Mac) and by PiaMacOS.
module MacInfoPlist
    # Default Info.plist content for macOS
    def self.defaultPlist(bundleName, executable, identifier)
        plistData = {}
        plistData['BuildMachineOSBuild'] = `sw_vers -buildVersion`.strip
        plistData['CFBundleDevelopmentRegion'] = 'en'
        plistData['CFBundleDisplayName'] = bundleName
        plistData['CFBundleExecutable'] = executable
        plistData['CFBundleIdentifier'] = identifier
        plistData['CFBundleInfoDictionaryVersion'] = '6.0'
        plistData['CFBundleName'] = bundleName
        plistData['CFBundlePackageType'] = 'APPL'
        plistData['CFBundleShortVersionString'] = Build::VersionBase
        plistData['CFBundleSignature'] = '????'
        plistData['CFBundleVersion'] = Build::VersionBase
        plistData['LSMinimumSystemVersion'] = "#{Build::MacosVersionMajor}.#{Build::MacosVersionMinor}"
        plistData['NSPrincipalClass'] = 'NSApplication'
        plistData
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
    def self.renderPlistXml(plistData)
        content = ''

        content << '<?xml version="1.0" encoding="UTF-8"?>' << "\n"
        content << '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' << "\n"
        content << '<plist version="1.0">' << "\n"
        renderPlistValue(content, plistData, '')
        content << '</plist>' << "\n"
        content
    end

    def self.renderDefaultPlist(bundleName, executable, identifier)
        renderPlistXml(defaultPlist(bundleName, executable, identifier))
    end
end
