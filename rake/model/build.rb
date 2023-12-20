require_relative '../util/util.rb'
require_relative '../util/dsl.rb'
require 'json'

# Set up a directory for building artifacts.
#
# Build determines the overall build directory (based on configuration, etc.)
# and creates a per-component build directory for this component.
class Build
    # Captures v3.4.5.beta.1-RC1 into groups [3, 4, 5, beta.1-RC1]
    version_regex = /v(\d+)\.(\d+)\.(\d+)(?:\.(.+))?/
    # Captures beta.1-RC1 into groups [beta.1, RC1]
    release_candidate_regex = /(.+)[-\/](.*)/
    # Retrieve the newest parent tag indicating the version.
    latestTag = `git tag -l --sort=-creatordate`.lines.find { |tag| tag.match(version_regex) }

    version_mmp, version_prerelease =
        if version_regex =~ ENV['PIA_OVERRIDE_VERSION'] || version_regex =~ latestTag
            version = [$1, $2, $3].map(&:to_i)
            pre_release = $4
            if release_candidate_regex =~ pre_release
                pre_release = $1
                puts "Ignoring version suffix #{$2}"
            end
            [version, pre_release]
        end

    # The major-minor-patch parts of this version
    VersionMMP = version_mmp || [3, 5, 2]
    # The base major-minor-patch version, as a string
    VersionBase = "#{VersionMMP[0]}.#{VersionMMP[1]}.#{VersionMMP[2]}"
    # The prerelease tags for this build (dot-separated, excluding leading
    # dash), or empty string if none
    VersionPrerelease = version_prerelease || ''

    # Select a build configuration based on environment variables, or use
    # defaults for the host platform if unspecified
    Brand = ENV['BRAND'] || 'pia'

    puts "Building #{Build::Brand} version #{VersionBase}#{"." if !VersionPrerelease.empty?}#{VersionPrerelease}"

    Variant = Util.selectSymbol('VARIANT', :debug, [:release, :debug])
    # Platform can be overridden to select mobile platforms, since mobile builds
    # are done from a desktop host.  Desktop cross-OS builds are not supported.
    Platform = Util.selectSymbol('PLATFORM', Util.hostPlatform,
                                 [:windows, :macos, :linux, :android, :ios, :iossim])
    # Don't allow unsupported cross-OS builds.  Android can be built from any
    # host.  iOS can only be built from macOS.  Desktop OSes can only be built
    # from the same host.
    if(Platform == :ios || Platform == :iossim)
        if(Util.hostPlatform != :macos)
            raise "ios build requires macos host"
        end
    elsif(Platform == :android)
        if(Util.hostPlatform == :windows)
            # This could be supported but currently isn't handled in clang.rb
            raise "android build currently requires macos or linux host"
        end
    else
        if(Util.hostPlatform != Platform)
            raise "#{Platform} build requires #{Platform} host"
        end
    end

    # Optionally set a compiler launcher like ccache or sccache.
    CompilerLauncher = ENV['COMPILER_LAUNCHER']
    if CompilerLauncher != nil
        puts "Using compiler launcher #{CompilerLauncher}"
    end

    # A number of convenience methods exist here to check for various
    # combinations of platforms.  Many of these combinations come up often, it
    # might seem a tad excessive here but it improves readability of the build
    # scripts overall.
    #
    # Note that throughout the build system and PIA Desktop product, "Linux"
    # refers to traditional "GNU/Linux" desktop systems, which doesn't include
    # Android.  (We require glibc on desktop, for example.)
    #
    # | Windows  |  macOS   |  Linux   | Android  |   iOS    |
    # |----------|----------|----------|----------|----------|
    # | windows? |  macos?  |  linux?  | android? |   ios?   |
    # |          |         <------- posix? ------->          | <-- "Unix-like"
    # |       <--- desktop? --->       |  <--- mobile? --->  | <-- "families"
    # |ntKernel? |xnuKernel?| <- linuxKernel? ->  |xnuKernel?| <-- Kernels
    #                                                 ^--- Includes iOS Sim.
    def self.windows?
        Platform == :windows
    end
    def self.macos?
        Platform == :macos
    end
    def self.linux?
        Platform == :linux
    end
    def self.android?
        Platform == :android
    end
    # An additional target exists for the iOS simulator, as separate SDKs exist
    # for iOS proper or the simulator (and they both support arm64, so we can't
    # just map "iOS/x86_64" to the simulator).  Android does not do this - there
    # is no separate "simulator" SDK.
    #
    # The only thing the :iossim platform does is change SDK (and output
    # directory), so there are no specific tests for the simulator.
    def self.ios?
        Platform == :ios || Platform == :iossim
    end
    def self.posix?
        Platform == :macos || Platform == :linux || Platform == :android || Platform == :ios || Platform == :iossim
    end
    def self.desktop?
        Platform == :windows || Platform == :macos || Platform == :linux
    end
    def self.mobile?
        Platform == :android || Platform == :ios || Platform == :iossim
    end
    def self.linuxKernel?
        Platform == :linux || Platform == :android
    end
    def self.xnuKernel?
        Platform == :macos || Platform == :ios || Platform == :iossim
    end
    # ntKernel? is a little silly (Windows is the only NT platform we support),
    # but it's here for contrast with the others
    def self.ntKernel?
        Platform == :windows
    end

    # Select a value based on platform, on desktop platforms only.
    # Used from desktop-specific builds only - raises an exeception when
    # building for mobile.
    def self.selectDesktop(winVal, macVal, linuxVal)
        return winVal if windows?
        return macVal if macos?
        return linuxVal if linux?
        raise "Cannot build desktop component when targeting #{Platform}"
    end
    # Select a value based on platform
    # For example:
    # binDir = Build.selectPlatform('/', 'Contents/MacOS', 'bin', 'bin', 'Contents/iOS')
    # This does not differentiate iOS vs. iOS Simulator.
    def self.selectPlatform(winVal, macVal, linuxVal, androidVal, iosVal)
        return winVal if windows?
        return macVal if macos?
        return linuxVal if linux?
        return androidVal if android?
        return iosVal if ios?
    end

    # Detect whether we can execute a binary for the current target, possibly
    # with emulation.
    def self.canExecute?
        # Different platform from host -> cannot execute
        return false if Platform != Util.hostPlatform
        # Same platform, host arch (exact or universal) -> can execute
        return true if TargetArchitecture == Util.hostArchitecture
        # Same platform, and "universal" arch (includes host arch) -> can execute
        return true if TargetArchitecture == :universal

        # Otherwise, this is a cross arch for the host platform.  Check if this
        # is possible on this platform.

        # Windows only supports x86_64 hosts, and either x86 or x86_64
        # targets.  Both can be executed.
        return true if windows?

        # The only cross execution possible on macOS is executing x86_64 on an
        # arm64 host via Rosetta 2.  This assumes that Rosetta 2 is installed.
        return true if macos? && TargetArchitecture == :x86_64 && Util.hostArchitecture == :arm64
        # Otherwise, cross execution is not possible on macOS.
        return false if macos?

        # On Linux, check if we can execute this architecture with arch-test.
        # We get one of three results:
        #   true: yes, we can execute this arch (possibly with emulation)
        #   false: no, we can't execute this arch
        #   nil: arch-test isn't present
        #
        # Ignore stdout, arch-test prints a message that doesn't make much
        # sense out of context if the arch isn't supported
        linuxArch = {
            x86_64: 'amd64',
            armhf: 'armhf',
            arm64: 'arm64'
        }[TargetArchitecture]
        result = system('arch-test', linuxArch, :out=>'/dev/null')
        puts "arch-test returned #{result} for #{linuxArch}"
        result
    end

    # For targets supporting a "universal" multi-arch target, these are the
    # architectures included in that build.
    PlatformUniversalArchitectures = {
        macos: [:x86_64, :arm64],
        ios: [:arm64, :arm64e],
        iossim: [:x86_64, :arm64]
    }

    # Select from supported architectures only.
    # To add an architecture:
    # - For complete desktop builds, build dependencies from desktop-dep-build
    #   for that arch and add to deps/built/.  (Not needed for kapps libs only.)
    # - Add support to the toolchain driver (clang: target names in
    #   rake/toolchain/clangplatform/<platform>.rb, msvc: VC arch name in
    #   rake/toolchain/msvc.rb)
    #
    # Apple platforms support a "universal" architecture that creates "fat"
    # builds including all supported architectures for that platform.  The
    # default is still the host machine arch only for dev workflow.
    PlatformSupportedArchitectures = {
        windows: [:x86, :x86_64],
        macos: PlatformUniversalArchitectures[:macos] + [:universal],
        linux: [:x86_64, :armhf, :arm64],
        android: [:x86, :x86_64, :armhf, :arm64],
        ios: PlatformUniversalArchitectures[:ios] + [:universal],
        iossim: PlatformUniversalArchitectures[:iossim] + [:universal]
    }
    # Default to the host architecture for everything except iOS:
    # - desktop - default to host for testing
    # - Android - default to host for testing in virtual device, all host archs
    #   are supported, even x86/x86_64
    # - iOS simulator - default to host for testing in simulator, supports both
    #   extant macOS architectures.
    #
    # iOS (non-Simulator)only supports arm64, so there's no sense defaulting to
    # the host arch for that build.
    DefaultArchitecture = (Platform == :ios) ? :arm64 : Util.hostArchitecture
    TargetArchitecture = Util.selectSymbol('ARCHITECTURE', DefaultArchitecture,
                                           PlatformSupportedArchitectures[Platform])


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
    # When building for the host OS, the platform is omitted (e.g.
    # 'out/pia_debug_x86_64'.  When building for a mobile OS, a platform is
    # included (e.g. 'out/pia_debug_android_x86').
    BuildPlatform = (Platform == Util.hostPlatform) ? "" : "#{Platform}_"
    BuildDir = "out/#{Brand}_#{Variant}_#{BuildPlatform}#{TargetArchitecture}#{ChrootSuffix}"

    # Minimum macOS version supported by the project.  (In addition to clang.rb,
    # also used in some generated Info.plist files.)
    MacosVersionMajor = 10
    MacosVersionMinor = 14

    # Shortcuts to check for specific build variants
    def self.debug?
        Variant == :debug
    end
    def self.release?
        Variant == :release
    end

    # Inject the BuildDSL dependency so we can stub it
    # out in specs
    def initialize(name, buildDsl = BuildDSL)
        @name = name

        # Build directory for this component
        @componentDir = File.join(BuildDir, @name)

        extend buildDsl

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
