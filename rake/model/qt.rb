require_relative 'build.rb'
require_relative 'component.rb'
require_relative 'probe.rb'
require_relative '../util/util.rb'

# Locate Qt to use:
# - Build-time tools: rcc, lupdate, moc, etc.
# - Qt modules, such as Qt5Core, Qt5Network, etc.
class Qt
    # Specify the preferred minor/patch version of Qt 5 here (the major version
    # must be 5).
    #
    # We use PIA's build of Qt on Linux; PIA may build with the qt.io releases
    # but we do not test it.  Windows and Mac still currently use the qt.io
    # builds but will likely switch to PIA's build in the future.
    #
    # - If the PIA build of the exact preferred version is available, it's used.
    # - Otherwise, if a PIA build of another patch level of the preferred minor
    #   version is available, the greatest available patch release of that minor
    #   version is used.
    # - Otherwise, the most recent PIA build is used.
    # - Otherwise, non-PIA Qt builds are checked using the same precedence.
    #
    # This allows us to control precisely when we apply Qt minor/patch updates,
    # while still allowing the product to be built with whatever version of Qt
    # is available.  (Later releases may likely work, but there are no
    # guarantees.)
    PreferredQtMinorVersion = 15
    PreferredQtPatchVersion = 0

    # Score biases to implement the preferences above
    ExactMatchBias = 2000000
    MinorMatchBias = 1000000
    PiaQtBuildBias =  500000

    def initialize()
        # Locate Qt.  QTROOT can be set to the path to a Qt version to force
        # the use of that build (if we can't find Qt automatically, or to force
        # a particular version)
        qtVersion = ENV['QTROOT']
        if(qtVersion != nil)
            if(!File.directory?(qtVersion))
                raise "QTROOT in environment does not exist: #{qtVersion}"
            end
        else
            searchRoots = []
            searchRoots << 'C:' if Build.windows?
            searchRoots << '/opt' if Build.linux?
            searchRoots << ENV['HOME'] if Build.posix?

            # Allow C:/Qt/... as well as C:/Qt5.12/..., etc.
            searchPatterns = Util.joinPaths([searchRoots, ['Qt*/5.*']])

            qtVersion = FileList[*searchPatterns].max_by do |p|
                # Both host and target Qt roots must be present
                hostQtRoot = getQtRoot(p, Util.hostArchitecture)
                targetQtRoot = getQtRoot(p, Build::TargetArchitecture)
                version = getQtPathVersion(p)
                if(hostQtRoot != nil && targetQtRoot != nil && version != nil)
                    # Prefer PIA Qt builds if available per above
                    piaBias = File.exist?(File.join(hostQtRoot, 'share/pia-qt-build')) ? PiaQtBuildBias : 0
                    getQtVersionScore(version[0], version[1]) + piaBias
                else
                    0
                end
            end
            if(qtVersion == nil || !File.directory?(qtVersion))
                raise "Unable to find Qt installation in #{searchPatterns}\n" +
                    "Install Qt and/or set QTROOT to the preferred Qt build, such as:\n" +
                    "QTROOT=#{File.join(searchRoots[0], "Qt/5.#{PreferredQtMinorVersion}.#{PreferredQtPatchVersion}")}"
            end
        end

        puts "Found Qt: #{qtVersion}"
        @actualVersion = getQtPathVersion(qtVersion)
        if(@actualVersion[0] != PreferredQtMinorVersion)
            # May not work, we haven't tested this minor version.  Qt tends to
            # deprecate functionality frequently in minor versions, and in
            # particular the Mac accessibility code fills in a lot of missing
            # internals from Qt
            puts "warning: Differs from preferred version 5.#{PreferredQtMinorVersion}.#{PreferredQtPatchVersion}"
        elsif(@actualVersion[1] != PreferredQtPatchVersion)
            # Minor version matches, but patch level is different.  This will
            # probably work, but we haven't tested it.
            # If the actual patch version is lower, it may reintroduce Qt bugs
            # that have been fixed.
            puts "note: Different patch number from preferred version 5.#{PreferredQtMinorVersion}.#{PreferredQtPatchVersion}"
        end

        # Determine the Qt build to use for the specified architecture.  Select
        # the toolchain Qt should be built with to make sure we don't pick up
        # something like a UWP/Android build on Windows, etc.
        #
        # Both host and target Qt builds are needed (build tools are used from
        # the host installation, libraries are used from the target).  These are
        # the same when not cross-compiling.
        @hostQtRoot = getQtRoot(qtVersion, Util.hostArchitecture)
        @targetQtRoot = getQtRoot(qtVersion, Build::TargetArchitecture)
        puts "Host: #{Util.hostArchitecture} - #{@hostQtRoot}"
        puts "Target: #{Build::TargetArchitecture} - #{@targetQtRoot}"

        # Use a probe to detect if the Qt directory changes
        qtProbe = Probe.new('qt')
        qtProbe.file('qtversion.txt', "5.#{@actualVersion[0]}.#{@actualVersion[1]}\n")
        qtProbe.file('qtroot.txt', "#{@hostQtRoot}\n#{@targetQtRoot}\n")
        qtProbeArtifact = qtProbe.artifact('qtroot.txt')

        if(@hostQtRoot == nil)
            raise "Unable to find any \"#{Util.hostArchitecture}\" Qt build in #{qtVersion}"
        end
        if(@targetQtRoot == nil)
            raise "Unable to find any \"#{Build::TargetArchitecture}\" Qt build in #{qtVersion}"
        end

        mkspec = ''
        mkspec = 'win32-msvc' if Build.windows? # For either 32 or 64 bit
        mkspec = 'macx-clang' if Build.macos?
        mkspec = 'linux-g++' if Build.linux?

        # Create the "core" component and add essential Qt definitions
        @core = buildDefaultComponent("Core", qtProbeArtifact)
            .include(File.join(@targetQtRoot, "include"))
            .include(File.join(@targetQtRoot, "mkspecs/#{mkspec}"))
            .libPath(File.join(@targetQtRoot, "lib"))
        @core.lib('qtmain') if Build.windows? && Build.release?
        @core.lib('qtmaind') if Build.windows? && Build.debug?
        if(Build.macos?)
            # Specify Qt framework path on Mac
            @core.frameworkPath(File.join(@targetQtRoot, "lib")) if Build.macos?
            # We should load these frameworks from the .prl files (see component()),
            # for now hard-code the common dependencies
            @core.framework('DiskArbitration')
            @core.framework('IOKit')
        end
        # Qt requires libdl and libpthread on Linux
        @core.lib('dl') if Build.linux?
        @core.lib('pthread') if Build.linux?
    end

    private

    def getQtToolchainPatterns(arch)
        # Some Qt releases on Windows have builds for more than one version of
        # MSVC.  Select the latest one.
        # '????' instead of '*' ensures we don't match 'msvc2017_64' when
        # looking for 'msvc2017'
        qtToolchains = ['msvc????'] if Build.windows?
        qtToolchains = ['clang'] if Build.macos?
        qtToolchains = ['clang', 'gcc'] if Build.linux?
        suffix = ''
        if(arch == :x86_64)
            suffix = '_64'
        elsif(arch != :x86)
            # Qt doesn't provide armhf or arm64 builds.  The PIA Qt builds
            # include the entire architecture name here.
            suffix = '_' + Build::TargetArchitecture.to_s
        end
        qtToolchains.map { |t| t + suffix }
    end

    def getQtRoot(qtVersion, arch)
        qtToolchainPtns = getQtToolchainPatterns(arch)
        qtRoots = FileList[*Util.joinPaths([[qtVersion], qtToolchainPtns])]
        # Explicitly filter for existing paths - if the pattern has wildcards
        # we only get existing directories, but if the patterns are just
        # alternates with no wildcards, we can get directories that don't exist
        qtRoots.find_all { |r| File.exist?(r) }.max
    end

    def getQtVersionScore(minor, patch)
        score = minor * 100 + patch
        if(minor == PreferredQtMinorVersion)
            if(patch == PreferredQtPatchVersion)
                score += ExactMatchBias # Exact match
            else
                score += MinorMatchBias # Matches minor release only
            end
        end
        score
    end

    def getQtPathVersion(path)
        verMatch = path.match('^.*/Qt[^/]*/5\.(\d+)\.?(\d*)$')
        if(verMatch == nil)
            nil
        else
            [verMatch[1].to_i, verMatch[2].to_i]
        end
    end

    # Build a component definition with the defaults.  The "Core" component will
    # get some essential dependencies added, other components may have
    # specific dependencies too.
    def buildDefaultComponent(name, task)
        comp = Component.new(task)
            .define("QT_#{name.upcase}_LIB")

        if(Build.windows?)
            comp.include(File.join(@targetQtRoot, "include/Qt#{name}"))
            comp.lib("Qt5#{name}") if Build.release?
            comp.lib("Qt5#{name}d") if Build.debug?
        elsif(Build.macos?)
            comp.include(File.join(@targetQtRoot, "lib/Qt#{name}.framework/Headers"))
            comp.framework("Qt#{name}")
        elsif(Build.linux?)
            comp.include(File.join(@targetQtRoot, "include/Qt#{name}"))
            comp.lib(File.join(@targetQtRoot, 'lib', "libQt5#{name}.so.5.#{@actualVersion[0]}.#{@actualVersion[1]}"))
        else
            raise "Don't know how to define Qt component #{name} for #{Build::Platform}"
        end

        comp
    end

    public

    # Get the Core component.  This must be used when using any other Qt modules
    def core()
        @core
    end

    # Get a Component representing a Qt module.  Pass the canonically-cased
    # name, such as "Network".
    #
    # The core module (Qt.core()) must be used when using any other modules.
    # Do not attempt to get the Core module from component(), use core().
    def component(name)
        # TODO: The Qt module in qbs handles this by reading the .pri files
        # from <qt>/mkspecs/modules, and the .prl files stored with the modules
        # themselves.
        #
        # The .pri files contain include directories, macros to define, and lib
        # paths.  These are systematic enough that they are hard-coded here for
        # now.  It also contains module dependencies, which are not handled
        # currently, the executable will just have to reference dependency
        # modules manually.
        #
        # The .prl files contain linker arguments for dependency libraries.
        # This is particularly important on macOS where many of the modules have
        # dependencies on extra system frameworks.  This isn't handled right
        # now, the common dependencies are attached to the core component, but
        # extra module-specific dependencies have to be referenced by the
        # executable.

        # Not a built component - found on the system.  No task name
        comp = buildDefaultComponent(name, nil)

        # Add specific known framework dependencies on Mac in lieu of reading
        # them from prl files for now.
        if(Build.macos?)
            comp.framework('Metal') if name == 'Quick'
            comp.framework('ApplicationServices') if name == 'Test'
        end

        comp
    end

    # Get the path to a tool, such as rcc, lupdate, moc, etc.  The tool name
    # should include the .exe extension on Windows.
    def tool(name)
        File.join(@hostQtRoot, 'bin', name).tap{|v| v << '.exe' if Build.windows?}
    end

    def targetQtRoot
        @targetQtRoot
    end
    def hostQtRoot
        @hostQtRoot
    end
end
