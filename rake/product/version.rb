require_relative '../model/probe.rb'
require_relative '../model/build.rb'
require_relative '../util/cppheader.rb'
require_relative '../util/util.rb'
require 'date'
require 'json'
require 'forwardable'

# Generate the brand and version files for PIA.  Consists of two steps:
# - a Probe to write basic information not collected from files - the version
#   number (hard-coded here) and information from Git and the environment
# - a Generator that additionally reads brand information from brand.json and
#   generates version.txt, version.h, and brand.h
class PiaVersion
    extend Forwardable

    def initialize
        variantSuffix = (Build::Variant == :release) ? "" : Build::Variant.to_s
        variantDot = (variantSuffix == '') ? '' : '.'

        @branchBuild = ENV['PIA_BRANCH_BUILD']
        if(@branchBuild == '')
            @branchBuild = nil
        end

        # Default to 0 if the environment variable is absent or empty (the .to_s
        # converts nil to '')
        @timestamp = ENV['SOURCE_DATE_EPOCH'].to_s.to_i

        @gitFound = File.exist?('.git/logs/HEAD')
        if(@gitFound)
            @shortRevision = `git describe --always --match=nosuchtagpattern`.strip
            # Use the branch from PIA_BRANCH_BUILD if it's set, otherwise fall
            # back to the Git branch
            if(@branchBuild != nil)
                @branch = @branchBuild
            else
                @branch = `git rev-parse --symbolic-full-name --abbrev-ref HEAD`.strip
            end
            @commitCount = `git rev-list --count HEAD`.strip
            while(@commitCount.size < 5) do @commitCount = "0#{@commitCount}" end
            @lastCommitTime = `git log -1 --format=%at`.strip

            # Prefer SOURCE_DATE_EPOCH for the timestamp if it was given
            if(@timestamp == 0)
                @timestamp = Integer(@lastCommitTime, 10)
            end

            # For releases (PIA_BRANCH_BUILD not set), just use the commit count
            # and variant (if any).  For branch builds (including master), use
            # the branch name, timestamp, and revision.
            if(@branchBuild == nil)
                @versionBuild = "#{@commitCount}#{variantDot}#{variantSuffix}"
            else
                datetime = Time.at(@timestamp).to_datetime.iso8601.gsub(/[^0-9]/, '')
                @versionBuild = "#{@branch.gsub(/[^A-Za-z0-9]/, '-')}.#{datetime}.#{@shortRevision}"
            end
        else
            # No git, all we can get is the build variant
            @versionBuild = variantSuffix
        end

        # Set both SOURCE_DATE_EPOCH (if it wasn't already set) and
        # QT_RCC_SOURCE_DATE_OVERRIDE (used by rcc <= 5.12)
        ENV['SOURCE_DATE_EPOCH'] = @timestamp.to_s
        ENV['QT_RCC_SOURCE_DATE_OVERRIDE'] = @timestamp.to_s

        @productName = Build::BrandInfo['brandName']
        @productShortName = Build::BrandInfo['shortName']

        packageNameParts = [ Build::Platform.to_s ]
        if(Build::Platform == :windows)
            if(Build::Architecture == :x86_64)
                packageNameParts.push("x64")
            else
                packageNameParts.push(Build::Architecture.to_s)
            end
        end
        packageNameParts.push(Util.deleteSuffix(Build::VersionBase, '.0'))
        if(Build::VersionPrerelease != '')
            packageNameParts.push(Build::VersionPrerelease)
        end
        if(versionBuild != '')
            packageNameParts.push(versionBuild)
        end

        @packageName = ([Build::Brand] + packageNameParts).join('-')
        @integtestPackageName = ([Build::Brand, 'integtest'] + packageNameParts).join('-')

        @probe = Probe.new('version')
        @probe.file('version.txt', "#{version}\n#{@productName}\n#{@packageName}\n#{@timestamp}\n")
        @probe.file('brand.txt', "#{@productName}\n#{Build::Brand}\n#{Build::ProductIdentifier}\n#{Build::BrandInfo['helpDeskLink']}\n")

        # Build version.h
        versionh = CppHeader.new('version')
        versionh.defineString('PIA_PRODUCT_NAME', @productName)
        versionh.defineString('PIA_VERSION', version)
        cert = ENV['PIA_CODESIGN_CERT']
        cert = "Unknown" if cert == nil
        versionh.defineString('PIA_CODESIGN_CERT', cert)
        migration = ENV['RUBY_MIGRATION']
        migration = '' if migration == nil
        versionh.defineRawString('RUBY_MIGRATION', migration)
        versionh.defineLiteral('INCLUDE_FEATURE_HANDSHAKE', '0')
        # Windows-style four-part version number used in VERSIONINFO.
        # This can't completely encode a semantic version, so we use the
        # fourth field to express alpha/beta/GA:
        # * 0 - all alphas (or any non-beta prerelease)
        # * 1-99 - beta.1 through beta.99
        # * 100 - GA
        winVerLast = '0'
        if(Build::VersionPrerelease == '')
            winVerLast = '100'
        else
            Build::VersionPrerelease.match(/^beta\.([0-9]?[0-9])$/) { |m| winVerLast = m[1] }
        end
        winVer = Build::VersionBase.gsub('.', ',') + ',' + winVerLast
        # Write it both literally and as a string, the RC preprocessor
        # doesn't seem to support the stringification operator.
        # Note that Qbs doesn't seem to create a dependency on version.h
        # for the RC script, so changes in version require a full rebuild.
        versionh.defineLiteral('PIA_WIN_VERSION', winVer)
        versionh.defineString('PIA_WIN_VERSION_STR', winVer)
        @probe.file('version.h', versionh.content)

        # Build brand.h
        brandh = CppHeader.new('brand')
        brandh.defineString('BRAND_NAME', @productName)
        brandh.defineString('BRAND_CODE', Build::Brand)
        brandh.defineString('BRAND_SHORT_NAME', @productShortName)
        brandh.defineString('BRAND_IDENTIFIER', Build::ProductIdentifier)
        brandh.defineLiteral('BRAND_HAS_CLASSIC_TRAY', Build::BrandInfo['brandHasClassicTray'] ? '1' : '0')
        brandh.defineLiteral('BRAND_MIGRATES_LEGACY', Build::Brand == 'pia' ? '1' : '0')
        brandh.defineString('BRAND_RELEASE_CHANNEL_GA', Build::BrandInfo['brandReleaseChannelGA'])
        brandh.defineString('BRAND_RELEASE_CHANNEL_BETA', Build::BrandInfo['brandReleaseChannelBeta'])
        brandh.defineString('BRAND_WINDOWS_PRODUCT_GUID', Build::BrandInfo['windowsProductGuid'])
        brandh.defineString('BRAND_WINDOWS_SERVICE_NAME', Build::BrandInfo['windowsServiceName'])
        brandh.defineString('BRAND_WINDOWS_WIREGUARD_SERVICE_NAME', Build::BrandInfo['windowsWireguardServiceName'])
        brandh.defineLiteral('BRAND_WINDOWS_WFP_PROVIDER', Build::BrandInfo['windowsWfpProvider'])
        brandh.defineString('BRAND_WINDOWS_WFP_PROVIDER_GUID', Build::BrandInfo['windowsWfpProviderGuid'])
        brandh.defineLiteral('BRAND_WINDOWS_WFP_SUBLAYER', Build::BrandInfo['windowsWfpSublayer'])
        brandh.defineString('BRAND_WINDOWS_WFP_SUBLAYER_GUID', Build::BrandInfo['windowsWfpSublayerGuid'])
        brandh.defineString('BRAND_UPDATE_JSON_KEY_NAME', Build::BrandInfo['updateJsonKeyName'])
        brandh.defineString('BRAND_LINUX_APP_NAME', Build::BrandInfo['linuxAppName'])
        brandh.defineLiteral('BRAND_LINUX_FWMARK_BASE', Build::BrandInfo['linuxFwmarkBase'].to_s)
        brandh.defineLiteral('BRAND_LINUX_CGROUP_BASE', Build::BrandInfo['linuxCgroupBase'].to_s)
        brandh.defineRawString('BRAND_PARAMS', JSON.generate(Build::BrandInfo))
        brandh.defineString('BRAND_WINTUN_AMD64_PRODUCT', Build::BrandInfo['wintunAmd64Product'])
        brandh.defineString('BRAND_WINTUN_X86_PRODUCT', Build::BrandInfo['wintunX86Product'])
        brandh.defineString('BRAND_WINTUN_PRODUCT_NAME', Build::BrandInfo['wintunProductName'])
        updateApis = Build::BrandInfo['brandReleaseUpdateUris']
        # This was added to the brand kit, default to the old defaults
        if(updateApis == nil)
            updateApis = [
                'https://www.privateinternetaccess.com/clients/desktop',
                'https://www.piaproxy.net/clients/desktop'
            ]
        end
        brandh.defineLiteral('BRAND_UPDATE_APIS',
            updateApis.map {|u| "QStringLiteral(R\"(#{u})\")"}.join(', '))
        @probe.file('brand.h', brandh.content)
    end

    # Export a component definition (for headers), or get an artifact path (for
    # text files typically)
    def_delegators :@probe, :export, :artifact

    # The build tags for this build (dot-separted, excluding leading +), or
    # empty string if none
    def versionBuild
        @versionBuild
    end
    # The complete semantic version for this release
    def version
        version = String.new(Build::VersionBase)
        if(Build::VersionPrerelease != '')
            version << "-#{Build::VersionPrerelease}"
        end
        if(versionBuild != '')
            version << "+#{versionBuild}"
        end
        version
    end
    # The branded product name ("Private Internet Access" in the PIA brand)
    def productName
        @productName
    end
    # The short name for the product ("PIA" in the PIA brand)
    def productShortName
        @productShortName
    end
    # The complete package name for this release
    def packageName
        @packageName
    end
    # Package name for integration test artifacts
    def integtestPackageName
        @integtestPackageName
    end
    def timestamp
        @timestamp
    end

    # Generate a branded file from an un-branded source by applying various
    # substitutions.  (Use in the body of a file task.)
    def brandFile(source, target)
        srcContent = File.read(source)
        srcContent.gsub!('{{BRAND_CODE}}', Build::Brand)
        srcContent.gsub!('{{BRAND_IDENTIFIER}}', Build::ProductIdentifier)
        srcContent.gsub!('{{PIA_PRODUCT_NAME}}', productName)
        srcContent.gsub!('{{BRAND_NAME}}', productName)
        srcContent.gsub!('{{BRAND_SHORT}}', productShortName)
        srcContent.gsub!('{{BRAND_ID}}', Build::ProductIdentifier)
        srcContent.gsub!('{{BRAND_HELPDESK_LINK}}', Build::BrandInfo['helpDeskLink'])
        # Used by Mac install helper
        srcContent.gsub!('{{PIA_CODESIGN_CERT}}', ENV['PIA_CODESIGN_CERT'] || '')
        srcContent.gsub!('{{PIA_PRODUCT_VERSION}}', Build::VersionBase)
        srcContent.gsub!('{{PIA_SEMANTIC_VERSION}}', version)
        File.write(target, srcContent)
    end
end
