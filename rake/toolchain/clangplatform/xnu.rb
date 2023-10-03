require_relative '../../model/build.rb'
require_relative '../../util/util.rb'
require_relative '../../util/dsl.rb'

# Platform-specific details for the clang toolchain on XNU platforms.
class ClangXnu
    include BuildDSL

    # Minimum platform version when targeting macOS
    MacosMinVersion = "#{Build::MacosVersionMajor}.#{Build::MacosVersionMinor}"
    # Minimum platform version when targeting iOS
    IosMinVersion = '12.0'

    DriverPlatformArchOpts = {
        macos: {
            x86_64: ['-target', "x86_64-apple-macosx#{MacosMinVersion}-macho"],
            arm64: ['-target', "arm64-apple-macosx#{MacosMinVersion}-macho"]
        },
        ios: {
            arm64: ['-target', "arm64-apple-ios#{IosMinVersion}"],
            arm64e: ['-target', "arm64e-apple-ios#{IosMinVersion}"]
        },
        iossim: {
            x86_64: ['-target', "x86_64-apple-ios#{IosMinVersion}-simulator"],
            arm64: ['-target', "arm64-apple-ios#{IosMinVersion}-simulator"]
        }
    }

    def initialize(toolchainPath)
        # We have to specify the SDK manually when invoking clang for iOS cross
        # targets - clang has a default for the host, but not for cross.  Find
        # this first as it's needed for all the later detections.  Not strictly
        # needed for macOS targets but good for robustness.
        xcSdkNames = {
            macos: 'macosx',
            ios: 'iphoneos',
            iossim: 'iphonesimulator'
        }
        @sdkRoot = `xcrun --show-sdk-path --sdk #{xcSdkNames[Build::Platform]}`.chomp('')
        if(@sdkRoot.empty?)
            raise "Unable to find SDK for target #{Build::Platform} (Xcode name: #{xcSdkNames[Build::Platform]})"
        end
        puts "Found SDK for #{Build::Platform}: #{@sdkRoot}"

        # Toolchains targeting macOS/iOS don't provide llvm-ar, they
        # provide BSD ar, which also knows how to create symbol TOCs
        # needed by this toolchain.
        @ar = File.join(toolchainPath, 'ar')
    end

    def ar
        @ar
    end

    # Get the driver options for the current platform and architecture.  These
    # are used a lot - not just in compile/link steps, but also when probing
    # header/lib search paths, etc.
    def driverTargetOpts(architecture)
        opts = []
        # Options specific to platform and architecture
        opts += DriverPlatformArchOpts[Build::Platform][architecture]
        # Specify SDK probed from xcrun, needed for cross builds, fine for host
        # builds
        opts += ['-isysroot', @sdkRoot] if Build.xnuKernel?

        # We provide bitcode for iOS builds:
        # * for iOS release, embed actual bitcode, as these builds can go to the
        #   App Store
        # * for iOS Simulator (even release), or iOS debug, just embed a marker
        #   to indicate that the tooling would have embedded bitcode for a
        #   corresponding shippable build
        #
        # We don't provide bitcode for macOS builds.
        if(Build::Platform == :ios && Build.release?)
            opts += ['-fembed-bitcode=all']
        elsif(!Build.macos?)
            opts += ['-fembed-bitcode=marker']
        end

        opts
    end

    def dynamicTargetExt
        ".#{Build::VersionBase}.dylib"
    end

    # XNU targets generate dynamic library symlinks like: .3.1.dylib, .3.dylib,
    # .dylib
    # Put .dylib after the version part
    def dynamicSymlinkExtParts
        ['', '.dylib']
    end

    def linkOpts(architecture)
        opts = [
            # The link architecture names are x86_64 and arm64, same as our
            # internal symbols
            '-arch', architecture.to_s,
            # Max header padding - ensures that codesign has enough space to
            # modify load commands when inserting the signature
            '-headerpad_max_install_names'
        ]
        opts += ['-dead_strip'] if Build.release?
        []
    end

    # When linking static libraries with coverage enabled, include the whole
    # library, not just referenced objects.  This ensures that all source files
    # that were compiled end up in the coverage output.
    def linkCoverageStaticLibBeginOpts
        ['-Wl,-all_load']
    end
    def linkCoverageStaticLibEndOpts
        # ld on macOS has an -noall_load but it's obsolete and ignored, the
        # -all_load option is applied to all libraries specified regardless
        # of the ordering on the command line.
        []
    end

    # When linking a dynamic library, the driver options vary by platform
    def dynamicLinkDriverOpts
        ['-dynamiclib', '-current_version', Build::VersionBase]
    end
    # When linking a dynamic library, the linker options provide
    # platform-specific information for the dynamic linker
    def dynamicLinkLinkerOpts(targetBasename)
        ['-install_name', "@executable_path/../Frameworks/#{targetBasename}"]
    end

    def decorateCFunction(symbol)
        # C functions are decorated with '_' by default on macOS (see GCC/clang
        # -fno-leading-underscore)
        "_#{symbol}"
    end

    def extractSymbols(targetFile)
        # Extract the symbols to a separate .dSYM bundle
       Util.shellRun 'dsymutil', '-o', "#{targetFile}.dSYM", targetFile
       Util.shellRun 'strip', '-S', targetFile
    end
end
