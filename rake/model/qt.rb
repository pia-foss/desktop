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
    # - If the preferred version exactly is available, it's used.
    # - Otherwise, if another patch level of the preferred minor version is
    #   available, the greatest available patch release of that minor version is
    #   used.
    # - Otherwise, the greatest available version is used.
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
                v = getQtPathVersion(p)
                (v == nil) ? 0 : getQtVersionScore(v[0], v[1])
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
        qtToolchain = ''
        # Some Qt releases on Windows have builds for more than one version of
        # MSVC.  Select the latest one.
        # '????' instead of '*' ensures we don't match 'msvc2017_64' when
        # looking for 'msvc2017'
        qtToolchain = 'msvc????' if Build.windows?
        qtToolchain = 'clang' if Build.macos?
        qtToolchain = 'gcc' if Build.linux?
        qtToolchain << '_64' if Build::Architecture == :x86_64

        @qtRoot = FileList[File.join(qtVersion, qtToolchain)].max

        # Use a probe to detect if the Qt directory changes
        qtProbe = Probe.new('qt')
        qtProbe.file('qtversion.txt', "5.#{@actualVersion[0]}.#{@actualVersion[1]}\n")
        qtProbe.file('qtroot.txt', "#{@qtRoot}\n")
        qtProbeArtifact = qtProbe.artifact('qtroot.txt')

        if(@qtRoot == nil)
            raise "Unable to find any \"#{qtToolchain}\" Qt build in #{qtVersion}"
        end

        mkspec = ''
        mkspec = 'win32-msvc' if Build.windows? # For either 32 or 64 bit
        mkspec = 'macx-clang' if Build.macos?
        mkspec = 'linux-g++' if Build.linux?

        # Create the "core" component and add essential Qt definitions
        @core = buildDefaultComponent("Core", qtProbeArtifact)
            .include(File.join(@qtRoot, "include"))
            .include(File.join(@qtRoot, "mkspecs/#{mkspec}"))
            .libPath(File.join(@qtRoot, "lib"))
        @core.lib('qtmain') if Build.windows? && Build.release?
        @core.lib('qtmaind') if Build.windows? && Build.debug?
        if(Build.macos?)
            # Specify Qt framework path on Mac
            @core.frameworkPath(File.join(@qtRoot, "lib")) if Build.macos?
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
            comp.include(File.join(@qtRoot, "include/Qt#{name}"))
            comp.lib("Qt5#{name}") if Build.release?
            comp.lib("Qt5#{name}d") if Build.debug?
        elsif(Build.macos?)
            comp.include(File.join(@qtRoot, "lib/Qt#{name}.framework/Headers"))
            comp.framework("Qt#{name}")
        elsif(Build.linux?)
            comp.include(File.join(@qtRoot, "include/Qt#{name}"))
            comp.lib(File.join(@qtRoot, 'lib', "libQt5#{name}.so.5.#{@actualVersion[0]}.#{@actualVersion[1]}"))
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
        File.join(@qtRoot, 'bin', name).tap{|v| v << '.exe' if Build.windows?}
    end

    def qtRoot
        @qtRoot
    end
end
