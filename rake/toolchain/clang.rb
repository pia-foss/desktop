require_relative '../model/build'
require_relative '../util/util'
require_relative '../util/dsl'

require_relative './compiler_database'
require_relative './clangplatform/linux' if Build.linuxKernel?
require_relative './clangplatform/xnu' if Build.xnuKernel?

class ClangToolchain
    include BuildDSL

    # Initialize the Clang toolchain driver.  This driver can initialize any
    # number of architectures (unlike MSVC).
    #
    # If Qt is provided, moc and rcc rules are generated, and Qt lib paths are
    # used in RPATH as needed.  Qt is not required, pass nil if not using Qt.
    def initialize(architectures, qt)
        @qt = qt

        @mocPath = @qt&.tool('moc')
        @rccPath = @qt&.tool('rcc')

        # For Android builds, use the toolchain from the NDK.
        if(Build.android?)
            ndkToolchain = ClangLinux.findAndroidNdkToolchain
            @clang = "#{ndkToolchain}/bin/clang"
            @clangpp = "#{ndkToolchain}/bin/clang++"
        # Otherwise, if CLANG_VERSION is set to a specific version ('7', etc.),
        # use that version of clang and clang++.  This is primarily for Linux,
        # where multiple versions of clang might be installed.
        elsif ENV['CLANG_VERSION']
            @clang = "clang-#{ENV['CLANG_VERSION']}"
            @clangpp = "clang++-#{ENV['CLANG_VERSION']}"
        # Otherwise, find the newest clang available.  This applies to Linux,
        # macOS, and iOS - note that iOS builds use the host toolchain from
        # macOS.
        #
        # Searching for clang++ conveniently excludes things like
        # clang-tblgen-<ver>, and we need clang++ anyway.
        else
            clangPatterns = Util.joinPaths([ENV['PATH'].split(':'), ['clang++', 'clang++-*']])
            @clangpp = FileList[*clangPatterns]
                .find_all { |p| File.exist?(p) }
                .max_by { |p| getClangVersion(`#{p} --version`) }
            # Path to clang - 'clang++'/'clang++-<ver>' -> 'clang'/'clang-<ver>'
            @clang = File.join(File.dirname(@clangpp), File.basename(@clangpp).gsub('clang++', 'clang'))
        end

        # Find the clang version and toolchain install location.  This works for
        # all targets and doesn't depend on any platform-specific driver
        # arguments.
        clangVersionOutput = `#{@clang} --version`
        @clangMajorVersion = getClangVersion(clangVersionOutput)
        # Get the toolchain installation directory - needed by unit tests for
        # llvm-profdata, etc.
        @toolchainPath = clangVersionOutput.match(/^InstalledDir: (.*)$/m)[1].strip
        # On some systems (Debian), clang --version reports the toolchain path as
        # /usr/bin, but /usr/bin/clang is actually a symlink to a versioned LLVM
        # directory, which is where the rest of the LLVM tools are, like llvm-profdata.
        # (On Debian, the LLVM symlinks in /usr/bin have version suffixes, like
        # llvm-profdata-7.)
        @toolchainPath = File.dirname(File.realpath(File.join(@toolchainPath, File.basename(@clang))))
        puts "Detected clang #{@clangMajorVersion} at #{@toolchainPath} (#{@clang}, #{@clangpp})"

        # Find the platform-specific details.  These are needed to find the
        # library and include search directories below, which depend on driver
        # arguments.
        @platform = ClangLinux.new(@toolchainPath) if Build.linuxKernel?
        @platform = ClangXnu.new(@toolchainPath) if Build.xnuKernel?

        # Find the default include and framework directories (which we need to
        # give to moc) for each architecture being built.  They probably don't
        # actually depend on the architecture, but we do need to pass a target
        # to clang, so this avoids making that assumption.
        #
        # This is the only thing we need to know the possible architectures for
        # ahead of time, if we built these on-demand we wouldn't need to decide
        # on architectures at this point at all.
        @mocCppDefIncludeOpts = {}
        architectures.each do |a|
            @mocCppDefIncludeOpts[a] = buildMocCppDefIncludeOpts(a)
        end
    end

    def getClangVersion(clangVersionOutput)
        clangVersionOutput.match(/(clang|Apple LLVM) version (\d+)\./)[2].to_i
    end

    def findDefaultIncludes(architecture)
        frameworkPaths = []
        includePaths = []
        `#{@clangpp} -E -x c++ #{@platform.driverTargetOpts(architecture).map{|a| "'#{a}'"}.join(' ')} - -v </dev/null 2>&1 >/dev/null`
            .match(/#include <\.\.\.> search starts here:.(.*).End of search list\./m)[1]
            .split("\n")
            .map do |s|
                trimmed = Util.deletePrefix(s, ' ')
                pathOnly = Util.deleteSuffix(trimmed, " (framework directory)")
                if(pathOnly.length < trimmed.length)
                    frameworkPaths << pathOnly # It's a framework path
                else
                    includePaths << pathOnly # It's a normal include directory
                end
            end
        {frameworkPaths: frameworkPaths, includePaths: includePaths}
    end

    def buildMocCppDefIncludeOpts(architecture)
        cppDefaultPaths = findDefaultIncludes(architecture)
        cppDefaultPaths[:frameworkPaths].map{|p| "-F#{p}"} +
            cppDefaultPaths[:includePaths].map{|p| "-I#{p}"}
    end

    def findLibSearchPaths(architecture)
        `#{@clang} #{@platform.driverTargetOpts(architecture).map{|a| "'#{a}'"}.join(' ')} --print-search-dirs`
            .match(/^libraries: =(.*)$/m)[1].split(':')
    end

    def targetExt(type)
        return @platform.dynamicTargetExt if type == :dynamic
        return '.a' if type == :static
        return '' if type == :executable
    end
    # On Mac and Linux, create conventional symlinks to dynamic libraries using
    # less specific version numbers.
    def symlinkExts(type)
        return [] unless type == :dynamic
        extParts = @platform.dynamicSymlinkExtParts
        # If the platform doesn't want any symlinks, don't generate any.
        return [] if extParts == nil
        versions = [
            ".#{Build::VersionMMP[0]}.#{Build::VersionMMP[1]}",
            ".#{Build::VersionMMP[0]}",
            ''
        ]
        versions.map {|v| "#{extParts[0]}#{v}#{extParts[1]}"}
    end

    # The only object extension on Mac/Linux is .o
    def objectExt(sourceFile)
        '.o'
    end

    def isObjectFile?(sourceFile)
        File.extname(sourceFile) == '.o'
    end

    # Macros always defined
    # TODO: Qt 5.15 has deprecated a lot of API elements from Qt 5.12 (note
    # that deprecated warnings are also issued by default now).  Silence
    # these for now to highlight actual errors, we'll resolve deprecations
    # when the build is working with 5.15
    Macros = [ 'QT_NO_DEPRECATED_WARNINGS' ]

    VariantMacros = {
        debug: [
            # None
        ],
        release: [
            'NDEBUG',
            'QT_NO_DEBUG'
        ]
    }

    # C++-specific compile options
    CppCompileOpts = [
        '-fexceptions'
    ] +
    (Build.linux? ?
        [
            # Use C++14 on Linux since CI builds are still done on Debian Stretch,
            # where libstdcpp lacks C++17 support in the standard library.
            # Allow C++17 language extensions though, there are many of these that
            # are useful and clang-7 properly supports them.
            '-std=c++14', '-Wno-c++17-extensions'
        ] :
        [
            # Use proper C++17 everywhere else.
            '-std=c++17'
        ])

    # Objective-C++-specific compile options (includes relevant C++ options)
    # Mac only
    ObjCppCompileOpts = [
        '-fexceptions',
        '-fobjc-exceptions',
        '-fobjc-arc',   # Enable ARC, required by our Objective-C++ code
        '-fobjc-arc-exceptions',
        '-std=c++17'
    ]

    # Compile options for C and C++ that vary by variant
    CompileOpts = {
        debug: [
            '-O0'
        ],
        release: [
            '-O2'
        ]
    }

    # Compile options for C and C++ for code coverage
    CoverageDriverOpts = ['-fprofile-instr-generate', '-fcoverage-mapping']

    # Check if code coverage is actually available on this build machine
    def coverageAvailable?
        # Require clang 6.0 to generate coverage output.  Only provide coverage
        # output builds including the host platform/arch (matching arch or
        # universal).
        #
        # We can't process coverage data on Ubuntu 16.04 - its LLVM is too old
        # (llvm-cov does not support "export", and it seems to fail to load the
        # merged output for some reason).
        #
        # It's possible this might actually work with some versions of clang
        # between 3.8 and 6.0, but they haven't been tested.
        #
        # There's also an issue with coverage support when cross-compiling in
        # Stretch - it does not seem to be possible to install libclang-7-dev
        # for the target architecture; it indirectly conflicts with clang-7 for
        # the host architecture.  Don't use coverage support for cross builds on
        # Linux (but do allow it on macOS for universal builds)
        return false if Build.linux? && Build::TargetArchitecture != Util.hostArchitecture
        @clangMajorVersion >= 6 && Build::Platform == Util.hostPlatform
    end

    # Test if a header is available on the host system.  This is rarely useful,
    # as most of the time a build should just fail if dependencies aren't
    # present.
    #
    # This is used for optional tools that aren't part of shipping artifacts, so
    # those tools can be skipped if their dependencies aren't installed.
    #
    # (Note that the MSVC toolchain doesn't currently provide this.)
    def sysHeaderAvailable?(path)
        # Try preprocessing a file that just says "#include <path>", this
        # succeeds if the path is present.
        system("echo '#include <#{path}>' | #{@clangpp} -E -x c++ - -v 2>/dev/null >/dev/null")
    end

    # Get the toolchain installation path - where other tools like llvm-profdata
    # can be found.  Only used if coverageAvailable?() returns true (the MSVC
    # toolchain does not provide this)
    def toolchainPath
        @toolchainPath
    end

    # Find a library in the toolchain's search path.  Usually only used to ship
    # a dynamic library provided by an SDK.  The file name must be an exact
    # file name (not 'pthread' to find 'libpthread.so', use 'libpthread.so').
    # Raises an error if the library is not found.
    #
    # Note that the MSVC toolchain does not currently provide this.
    def findLibrary(architecture, libFileName)
        # Get the library search path from clang.  We could cache by
        # architecture, but this isn't used very much anyway.
        libSearchPaths = findLibSearchPaths(architecture)

        libDirIdx = libSearchPaths.index {|l| File.exist?(File.join(l, libFileName))}
        if(libDirIdx == nil)
            raise "Libary #{libFileName} does not exist in library search paths:\n  " +
                libSearchPaths.join('\n  ')
        end
        File.join(libSearchPaths[libDirIdx], libFileName)
    end

    # If coverage is requested and avaiable, returns opts (used for driver opts
    # and linker opts).  Otherwise, returns [].
    def coverageOpts(architecture, requested, opts)
        (requested && coverageAvailable?) ? opts : []
    end

    # Apply moc to a source or header file.  All paths should be absolute paths.
    def moc(architecture, sourceFile, outputFile, macros, includeDirs, frameworkPaths)
        if(@mocPath == nil)
            raise "Cannot run moc on #{sourceFile} when not using Qt"
        end
        params = [@mocPath]
        params += Macros.map { |m| "-D#{m}" }
        params += macros.map { |m| "-D#{m}" }
        params += includeDirs.map { |d| "-I#{d}" }
        # Indicate system include/framework directories explicitly
        params += @mocCppDefIncludeOpts[architecture]
        params += [
            '-o', outputFile,
            sourceFile
        ]

        Util.shellRun *params
    end

    # Invoke rcc to compile QRC resources to a source file
    def rcc(sourceFile, outputFile, name)
        if(@rccPath == nil)
            raise "Cannot run rcc on #{sourceFile} when not using Qt"
        end
        params = [
            @rccPath,
            sourceFile,
            '-name', name,
            '-o', outputFile
        ]

        Util.shellRun *params
    end

    def compileForLang(clang, architecture, langCompileOpts, sourceFile,
                       objectFile, depFile, macros, includeDirs, frameworkPaths,
                       coverage)
        params = [Build::CompilerLauncher, clang].compact
        params += langCompileOpts
        params += [
            '-g',   # Generate debug information
            '-Wall',
            '-Wextra',
            '-pipe',
            '-fvisibility=hidden', # Exports from dynamic libraries are explicitly marked
            '-Wno-unused-parameter',
            '-Wno-enum-constexpr-conversion',
            '-Wno-dangling-else',
            '-Werror=unused-result',
            '-Werror=return-type',
            '-fPIC',
            '-MMD', # Write dependency file for user headers only
            '-MF', depFile # Specify dependency file
        ]
        params += @platform.driverTargetOpts(architecture)
        params += CompileOpts[Build::Variant]
        params += coverageOpts(architecture, coverage, CoverageDriverOpts)
        params += frameworkPaths.map { |f| "-F#{f}" }
        params += Macros.map { |m| "-D#{m}" }
        params += VariantMacros[Build::Variant].map { |m| "-D#{m}" }
        params += macros.map { |m| "-D#{m}" }
        params += includeDirs.map { |d| "-I#{d}" }
        params += [
            '-o', objectFile,
            '-c', File.absolute_path(sourceFile) # Absolute path allows Qt Creator to open files from diagnostics
        ]

        Util.shellRun *params
        CompilerDatabase.create_fragment(sourceFile, objectFile, params)
    end

    # Compile one source file to an object file.  All paths should be absolute
    # paths.
    # The :coverage option is used in this step.
    def compile(architecture, sourceFile, objectFile, depFile, macros, includeDirs,
                frameworkPaths, options)
        ext = File.extname(sourceFile)
        if(ext == '.c')
            compileForLang(@clang, architecture, [], sourceFile, objectFile,
                           depFile, macros, includeDirs, frameworkPaths,
                           options[:coverage])
        elsif(ext == '.cpp' || ext == '.cc')
            compileForLang(@clangpp, architecture, CppCompileOpts, sourceFile,
                           objectFile, depFile, macros, includeDirs,
                           frameworkPaths, options[:coverage])
        # Objective-C++ source for macOS/iOS
        elsif(ext == '.mm' && Build.xnuKernel?)
            compileForLang(@clangpp, architecture, ObjCppCompileOpts,
                           sourceFile, objectFile, depFile, macros, includeDirs,
                           frameworkPaths, options[:coverage])
        # Assembling is supported on XNU targets to embed plists in text sections
        elsif(ext == '.s' && Build.xnuKernel?)
            # There's no way to specify additional assembler include directories
            # currently.
           Util.shellRun 'as', *@platform.driverTargetOpts(architecture), '-o', objectFile, sourceFile
        else
            raise "Don't know how to compile #{sourceFile}"
        end
    end

    def cppLink(architecture, targetFile, objFiles, libPaths, libs, frameworkPaths,
                frameworks, driverArgs, linkerArgs, coverage, forceLinkSymbols)
        linkParams = []
        # Include '$ORIGIN/../lib' in the rpath to find PIA-specific libraries
        # (specifically pia-clientlib) when testing builds from the staging
        # directory. 
        pathOrigin = ""
        if Build.linuxKernel?
            # On linux, `$ORIGIN` resolves to the path of the binary being loaded.
            pathOrigin = '$ORIGIN/../lib'
        elsif Build.xnuKernel?
            # On Mac, `@loader_path` resolves to the path of the binary being loaded.
            pathOrigin = '@loader_path/../Frameworks' 
        end
        linkParams.concat(['-rpath', "#{pathOrigin}"])
        # If Qt is used, add the Qt installation to the rpath.  (Otherwise, we
        # don't need to set an rpath.)  During the deploy step, rpaths will be
        # patched to the final installation directory; this rpath is needed for
        # directly running the staged build in development.
        if(libs.any? {|l| l.include?('Qt5Core')} || frameworks.any? {|f| f.include?('QtCore')})
            # Qt must be provided when linking to Qt
            if(@qt == nil)
                raise "Cannot link to Qt libraries when Qt was not passed to clang toolchain driver"
            end
            linkParams.concat(['-rpath', File.join(@qt.targetQtRoot, 'lib')])
        end
        linkParams.concat(@platform.linkOpts(architecture))
        linkParams.concat(linkerArgs)

        puts "link #{coverage ? "(with code coverage) " : ""}- #{targetFile}"

        params = [
            Build::CompilerLauncher,
            @clangpp,
            "-Wl,#{linkParams.join(',')}",
        ].compact
        params += driverArgs
        params += @platform.driverTargetOpts(architecture)
        params += coverageOpts(architecture, coverage, CoverageDriverOpts)
        params += libPaths.map { |l| "-L#{l}" }
        params += frameworkPaths.map { |f| "-F#{f}" }
        params += ['-o', targetFile]
        params += objFiles
        params += coverageOpts(architecture, coverage, @platform.linkCoverageStaticLibBeginOpts)
        # 'libs.uniq' eliminates duplicate libraries; e.g. if more than one
        # referenced library references libpthread, just specify it once.
        # Specifying -lpthread more than once causes link errors on Debian
        # Stretch.
        params += libs.uniq.map { |l| l.include?('/') ? l : "-l#{l}" }
        params += coverageOpts(architecture, coverage, @platform.linkCoverageStaticLibEndOpts)
        params += frameworks.uniq.flat_map { |f| ['-framework', f] }
        params += forceLinkSymbols.flat_map { |s| ['--force-link', @platform.decorateCFunction(s)] }

        Util.shellRun *params

        @platform.extractSymbols(targetFile)
    end

    # Link a target.
    # The :type and :coverage options are used in this step
    def link(architecture, targetFile, objFiles, libPaths, libs, frameworkPaths,
             frameworks, extraArgs, options)
        if(options[:type] == :dynamic)
            dynamicLinkOpts = @platform.dynamicLinkLinkerOpts(File.basename(targetFile))
            dynamicDriverOpts = @platform.dynamicLinkDriverOpts

            cppLink(architecture, targetFile, objFiles, libPaths, libs,
                    frameworkPaths, frameworks, dynamicDriverOpts,
                    dynamicLinkOpts + extraArgs, options[:coverage],
                    options[:forceLinkSymbols])
        elsif(options[:type] == :executable)
            cppLink(architecture, targetFile, objFiles, libPaths, libs,
                    frameworkPaths, frameworks, [], extraArgs, options[:coverage],
                    options[:forceLinkSymbols])
        elsif(options[:type] == :static)
            # Remove the existing static library before archiving.
            # The ar 'r' (replace) operation otherwise could leave
            # existing members in the archive that were supposed to
            # be removed.
            File.delete(targetFile) if File.exist?(targetFile)
            Util.shellRun @platform.ar, 'rcs', targetFile, *objFiles
        end
    end
end
