require_relative '../../model/build.rb'
require_relative '../../util/util.rb'
require_relative '../../util/dsl.rb'

# Platform-specific details for the clang toolchain on Linux-kernel platforms.
class ClangLinux
    include BuildDSL

    # Minimum API level supported on Android (determines the toolchain targets)
    AndroidApiVersion = 21

    DriverPlatformArchOpts = {
        linux: {
            x86_64: ['-target', 'x86_64-linux-gnu'],
            armhf: ['-target', 'arm-linux-gnueabihf'],
            arm64: ['-target', 'aarch64-linux-gnu']
        },
        android: {
            x86: ['-target', "i686-linux-android#{AndroidApiVersion}"],
            x86_64: ['-target', "x86_64-linux-android#{AndroidApiVersion}"],
            armhf: ['-target', "armv7a-linux-androideabi#{AndroidApiVersion}"],
            arm64: ['-target', "aarch64-linux-android#{AndroidApiVersion}"]
        }
    }
    # Preferred NDK version for Android - matches that used by our Android apps
    AndroidPreferredNdk = [21, 0, 6113669]

    def initialize(toolchainPath)
        # When targeting Linux/Android, use llvm-ar.  Android prebuilt
        # toolchains only provide llvm-ar.  For Linux desktop, ar is
        # usually also available and practically equivalent, but we
        # need other LLVM tools anyway (llvm-cov, etc.), so stick to
        # LLVM.
        @ar = File.join(toolchainPath, 'llvm-ar')
        # On most distributions, clang packages only include clang,
        # make sure LLVM is installed too.
        if(!File.exist?(@ar))
            raise "Found clang installed at #{toolchainPath}, but did not find LLVM tools.\n" +
                "Install an LLVM package, usually 'llvm' or 'llvm-<version>'"
        end
    end

    def ar
        @ar
    end

    # Get the driver options for the current platform and architecture.  These
    # are used a lot - not just in compile/link steps, but also when probing
    # header/lib search paths, etc.
    def driverTargetOpts(architecture)
        DriverPlatformArchOpts[Build::Platform][architecture]
    end

    # Linux desktop uses versioned dynamic libraries, but Android
    # conventionally does not.  (It works if they're deployed correctly, but
    # Android build tools don't deply them since they're not expected.)
    def dynamicTargetExt
        return ".so" if Build.android?
        ".so.#{Build::VersionBase}"
    end

    def dynamicSymlinkExtParts
        return nil if Build.android? # No symlinks on Android
        # Linux desktop conventionally uses dynamic library symlinks like:
        # .so.3.1, .so.3, .so
        # Put .so before the version part and nothing after
        ['.so', '']
    end

    def linkOpts(architecture)
        return ['--gc-sections'] if Build.release?
        []
    end

    # When linking static libraries with coverage enabled, include the whole
    # library, not just referenced objects.  This ensures that all source files
    # that were compiled end up in the coverage output.
    def linkCoverageStaticLibBeginOpts
        ['-Wl,--whole-archive']
    end
    def linkCoverageStaticLibEndOpts
        # ld on Linux applies --whole-archive to each subsequent library
        # on the command line, and it's recommended to turn it off before
        # system libraries are added by the driver.
        ['-Wl,--no-whole-archive']
    end

    # When linking a dynamic library, the driver options vary by platform
    def dynamicLinkDriverOpts
        ['-shared'] # Just indicate a dynamic library target
    end
    # When linking a dynamic library, the linker options provide
    # platform-specific information for the dynamic linker
    def dynamicLinkLinkerOpts(targetBasename)
        ["-soname=#{targetBasename}", '--as-needed']
    end

    def decorateCFunction(symbol)
        symbol # No decoration occurs for C names on Linux platforms
    end

    def extractSymbols(targetFile)
        # No symbol extraction occurs on Linux
    end

    ###############
    # Android NDK #
    ###############

    # When targeting Android, an NDK version is chosen.  Releases are made with
    # the preferred version above, but allow the build to continue with any
    # installed version if necessary.
    #
    # If the exact preferred version is present, use that.  Othewrise, we will
    # use the latest matching major.minor, or the latest matching major, or the
    # latest release of any later major version.
    #
    # These constants are used to implement this precedence order by scoring
    # each available versions.
    #
    # The major-minor-patch is combined into one value for the vesion selection.
    # It's not clear what exactly the NDK patch numbers mean, but they always
    # seem to be 7 digits, so use 1e8 to provide some headroom.
    AndroidNdkVersionMinorFactor = 100_000_000
    # Assume minor is always <100, seems to be sequential for each release of
    # that major version; rarely exceeds 9.
    AndroidNdkVersionMajorFactor = 100 * AndroidNdkVersionMinorFactor
    # Exact match is preferred over all others
    AndroidNdkVersionExactBias = 3000 * AndroidNdkVersionMajorFactor
    # Major-minor match is preferred after exact match
    AndroidNdkVersionMinorBias = 2000 * AndroidNdkVersionMajorFactor
    # Major match is preferred next
    AndroidNdkVersionMajorBias = 1000 * AndroidNdkVersionMajorFactor
    def self.getAndroidNdkVersion(path)
        verMatch = path.match('^.*/(\d+)\.(\d+)\.(\d+)$')
        if(verMatch == nil)
            nil
        else
            [verMatch[1].to_i, verMatch[2].to_i, verMatch[3].to_i]
        end
    end
    def self.scoreAndroidNdkVersion(path)
        ver = getAndroidNdkVersion(path)
        if(ver == nil)
            return 0
        end

        verScore = ver[0] * AndroidNdkVersionMajorFactor +
            ver[1] * AndroidNdkVersionMinorFactor + ver[2]
        # Bias for exact match
        if(ver[0] == AndroidPreferredNdk[0] && ver[1] == AndroidPreferredNdk[1] &&
            ver[2] == AndroidPreferredNdk[2])
            verScore += AndroidNdkVersionExactBias
        # Bias for major-minor match
        elsif(ver[0] == AndroidPreferredNdk[0] && ver[1] == AndroidPreferredNdk[1])
            verScore += AndroidNdkVersionMinorBias
        # Bias for major match
        elsif(ver[0] == AndroidPreferredNdk[0])
            verScore += AndroidNdkVersionMajorBias
        # Otherwise, not even major matches.  Only allow newer major versions,
        # not older (drop score to 0 for older)
        elsif(ver[0] < AndroidPreferredNdk[0])
            verScore = 0
        end
        verScore
    end

    # Locate the Android NDK toolchain according to our preferences and the
    # NDKROOT override.  Returns a path to the toolchain installation directory
    # (parent of bin/, lib/, etc.), such as:
    #    "/home/<user>/Android/Sdk/ndk/<version>/toolchains/llvm/prebuilt/<host>"
    #
    # If no suitable toolchain is found, raises an error (fails the build)
    def self.findAndroidNdkToolchain
        # NDKROOT can override the detected NDK path
        ndkVersionRoot = ENV['NDKROOT']
        if(ndkVersionRoot != nil && !File.directory?(ndkVersionRoot))
            raise "NDKROOT in environment does not exist: #{ndkVersionRoot}"
        end

        # If the override was not set, detect automatically
        if(ndkVersionRoot == nil)
            # This is where Android Studio installs NDKs
            ndkInstallRoots = []
            ndkInstallRoots << "#{ENV['HOME']}/Android/Sdk/ndk" if Util.hostPlatform == :linux
            # CI builds are performed on Linux; we symlink the installing user's
            # ~/Android to /opt/Android since CI runs as the gitlab-ci user.
            ndkInstallRoots << "/opt/Android/Sdk/ndk" if Util.hostPlatform == :linux
            # On macOS, it's in ~/Library rather than ~/
            ndkInstallRoots << "#{ENV['HOME']}/Library/Android/sdk/ndk" if Util.hostPlatform == :macos
            # This may work on Windows with the right search prefix, but it
            # hasn't been tested.

            if(ndkInstallRoots.empty?)
                raise "NDK install location not known for host platform #{Util.hostPlatform}\n" +
                    "Specify NDK with NDKROOT or add install location for #{Util.hostPlatform} to rake/toolchain/clang.rb"
            end
            ndkSearchPatterns = Util.joinPaths([ndkInstallRoots, ['*']])
            ndkVersionRoot = FileList[*ndkSearchPatterns].max_by do |p|
                scoreAndroidNdkVersion(p)
            end
        end

        # Text for preferred version, for output
        ndkPrefVer = "#{AndroidPreferredNdk[0]}.#{AndroidPreferredNdk[1]}.#{AndroidPreferredNdk[2]}"
        # Validate the detected (or overridden) NDK.
        # If there was no match, or the only match was unacceptable (score
        # 0, like an older major version), we can't build.
        if(ndkVersionRoot == nil || scoreAndroidNdkVersion(ndkVersionRoot) == 0)
            raise "Unable to find Android NDK >= #{ndkPrefVer}\n" +
                "Install NDK and/or set NDKROOT to the preferred NDK, such as:\n" +
                "NDKROOT=<...>/Android/Sdk/ndk/#{ndkPrefVer}\""
        end
        ndkVer = getAndroidNdkVersion(ndkVersionRoot)
        # Warn if it's not our preferred version - probably will work, but
        # use at your own risk
        if(ndkVer[0] != AndroidPreferredNdk[0] || ndkVer[1] != AndroidPreferredNdk[1] || ndkVer[2] != AndroidPreferredNdk[2])
            puts "warning: NDK #{ndkVer[0]}.#{ndkVer[1]}.#{ndkVer[2]} differs from preferred version #{ndkPrefVer}"
        end

        # Find the prebuilt LLVM toolchain.  This depends on the host
        # architecture - it's not clear if there's any reason more than one
        # host architecture build would ever be installed.
        ndkLlvmToolchains = FileList["#{ndkVersionRoot}/toolchains/llvm/prebuilt/*"]
        if(ndkLlvmToolchains.length < 1)
            raise "Detected NDK at #{ndkVersionRoot} has no LLVM toolchain\n" +
                "Repair the NDK installation, or specify a different NDK by setting NDKROOT"
        end
        # There should just be one, we'll warn if there isn't.  Use 'min'
        # arbitrarily in that case; avoids depending on filesystem order.
        ndkToolchain = ndkLlvmToolchains.min
        if(ndkLlvmToolchains.length > 1)
            # We'll pick one arbitrarily; warn because this isn't completely
            # handled - if it happens, we should probably actually detect
            # the host OS/arch and pick the correct toolchain.
            puts "warning: NDK #{ndkVer[0]}.#{ndkVer[1]}.#{ndkVer[2]} contains #{ndkLlvmToolchains.length} toolchains, chose:\n" +
                ndkToolchain
        end
        ndkToolchain
    end
end
