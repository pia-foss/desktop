require_relative '../util/util.rb'
require_relative '../util/dsl.rb'
require 'json'

# Set up a directory for building artifacts.
#
# Build determines the overall build directory (based on configuration, etc.)
# and creates a per-component build directory for this component.
class Build
    # The major-minor-patch parts of this version
    VersionMMP = [2, 10, 0]
    # The base major-minor-patch version, as a string
    VersionBase = "#{VersionMMP[0]}.#{VersionMMP[1]}.#{VersionMMP[2]}"
    # The prerelease tags for this build (dot-separated, excluding leading
    # dash), or empty string if none
    VersionPrerelease = ''

    # Select a build configuration based on environment variables, or use
    # defaults for the host platform if unspecified
    Brand = ENV['BRAND'] || 'pia'
    Variant = Util.selectSymbol('VARIANT', :debug, [:release, :debug])
    # Platform can't be overridden, only native builds are supported
    Platform = Util.hostPlatform
    TargetArchitecture = Util.selectSymbol('ARCHITECTURE', Util.hostArchitecture,
                                           [:x86, :x86_64, :armhf, :arm64])

    # Load the essential brand info - the brand identifier is needed in many
    # places on Mac to brand assets, property lists, etc.
    # More complete parsing is done by PiaVersion, which generates version.h,
    # brand.h, etc.
    BrandInfo = JSON.parse(File.read(File.join('brands', Brand,
                           'brandinfo.json')))
    # The DNS-style identifier for this product
    # ('com.privateinternetaccess.vpn' in the PIA brand)
    ProductIdentifier = BrandInfo['brandIdentifier']

    # If building in a piabuild chroot environment on Linux, include the
    # environment name so the output directory does not conflict with host
    # builds.
    # For example, in a 'piabuild-stretch-<arch>' chroot, set the output
    # directory to 'pia_<variant>_<arch>_stretch'.
    chrootPiabuildMatch = nil
    chrootPiabuildMatch = ENV['SCHROOT_CHROOT_NAME'].match('^piabuild-([^-]*)-*.*$') if ENV['SCHROOT_CHROOT_NAME']
    ChrootSuffix = chrootPiabuildMatch ? "_#{chrootPiabuildMatch[1]}" : ''

    # Determine the build directory based on configuration
    BuildDir = "out/#{Brand}_#{Variant}_#{TargetArchitecture}#{ChrootSuffix}"

    # Minimum macOS version supported by the project
    MacosVersionMajor = 10
    MacosVersionMinor = 13

    # Convenience methods to check for specific variants/platforms (or for
    # "posix", includes Mac and Linux)
    def self.debug?
        Variant == :debug
    end
    def self.release?
        Variant == :release
    end
    def self.windows?
        Platform == :windows
    end
    def self.macos?
        Platform == :macos
    end
    def self.linux?
        Platform == :linux
    end
    def self.posix?
        Platform == :macos || Platform == :linux
    end

    # Select a value based on platform
    # For example:
    # binDir = Build.selectPlatform('/', 'Contents/MacOS', 'bin')
    def self.selectPlatform(winVal, macVal, linuxVal)
        return winVal if windows?
        return macVal if macos?
        return linuxVal if linux?
    end

    include BuildDSL

    def initialize(name)
        @name = name

        # Build directory for this component
        @componentDir = File.join(BuildDir, @name)

        # Create the component build directory
        directory @componentDir
    end

    # Get the component build directory - use this to add a dependency on this
    # task
    def componentDir
        @componentDir
    end

    # Get the path to an artifact built for this component.  The path can
    # include subdirectories
    def artifact(path)
        File.join(@componentDir, path)
    end
end
