require_relative '../model/build.rb'
require_relative '../util/util.rb'
require_relative '../util/dsl.rb'

# This toolchain would likely work with g++ too with minor modifications, but
# we only use it with clang.
class ClangToolchain
    include BuildDSL

    def initialize(qt)
        @qt = qt
        @mocPath = @qt.tool('moc')
        @rccPath = @qt.tool('rcc')

        @clang = 'clang' # In PATH
        @clangpp = 'clang++' # In PATH
        @mocCppDefIncludeOpts = findDefaultIncludes(@clangpp, 'c++')

        clangVersionOutput = `#{@clangpp} --version`
        @clangMajorVersion = clangVersionOutput.match(/(clang|Apple LLVM) version (\d+)\./)[2].to_i
        # Get the toolchain installation directory - needed by unit tests for
        # llvm-profdata, etc.
        @toolchainPath = clangVersionOutput.match(/^InstalledDir: (.*)$/m)[1].strip
        puts "Detected clang #{@clangMajorVersion} at #{@toolchainPath}"
    end

    def findDefaultIncludes(clang, lang)
        `#{clang} -E -x #{lang} - -v </dev/null 2>&1 >/dev/null`
            .match(/#include <\.\.\.> search starts here:.(.*).End of search list\./m)[1]
            .split("\n")
            .map do |s|
                pathOnly = Util.deleteSuffix(s, " (framework directory)")
                if(pathOnly.length < s.length)
                    "-F#{pathOnly}" # It's a framework path
                else
                    "-I#{pathOnly}" # It's a normal include directory
                end
            end
    end

    def targetExt(type)
        return ".so.#{Build::VersionBase}" if type == :dynamic && Build.linux?
        return ".#{Build::VersionBase}.dylib" if type == :dynamic && Build.macos?
        return '.a' if type == :static
        return '' if type == :executable
    end
    # On Mac and Linux, create conventional symlinks to dynamic libraries using
    # less specific version numbers.
    def symlinkExts(type)
        return [] unless type == :dynamic
        versions = [
            ".#{Build::VersionMMP[0]}.#{Build::VersionMMP[1]}",
            ".#{Build::VersionMMP[0]}",
            ''
        ]
        return versions.map {|v| ".so#{v}"} if Build.linux?
        versions.map{|v| "#{v}.dylib"} if Build.macos?
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

    # Driver arguments that vary by platform
    DriverPlatformOpts = {
        linux: [
            '-target', 'x86_64-pc-linux-gnu'
        ],
        macos: [
            '-stdlib=libc++',
            '-target', 'x86_64-apple-macosx10.10-macho'
        ]
    }

    # C++-specific compile options
    CppCompileOpts = [
        '-fexceptions',
        # Use C++14 on Linux since CI builds are still done with clang 3.8
        # on Ubuntu 16.04
        Build.linux? ? '-std=c++14' : '-std=c++17'
    ]

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

    LinkPlatformVariantOpts = {
        linux: {
            debug: [], # None
            release: ['--gc-sections']
        },
        macos: {
            debug: [], # None
            release: ['-dead_strip']
        }
    }

    LinkPlatformOpts = {
        linux: [
            # Linker script hard-coded for x86_64
            '-m', 'elf_x86_64'
        ],
        macos: [
            '-arch', 'x86_64',
            '-macosx_version_min', "#{Build::MacosVersionMajor}.#{Build::MacosVersionMinor}",
            # Max header padding - ensures that codesign has enough space to
            # modify load commands when inserting the signature
            '-headerpad_max_install_names'
        ]
    }

    # When linking static libraries with coverage enabled, include the whole
    # library, not just referenced objects.  This ensures that all source files
    # that were compiled end up in the coverage output.
    LinkCoverageStaticLibBeginOpts = {
        linux: [
            '-Wl,--whole-archive'
        ],
        macos: [
            '-Wl,-all_load'
        ]
    }
    LinkCoverageStaticLibEndOpts = {
        linux: [
            # ld on Linux applies --whole-archive to each subsequent library
            # on the command line, and it's recommended to turn it off before
            # system libraries are added by the driver.
            '-Wl,--no-whole-archive'
        ],
        macos: [
            # ld on macOS has an -noall_load but it's obsolete and ignored, the
            # -all_load option is applied to all libraries specified regardless
            # of the ordering on the command line.
        ]
    }

    # Check if code coverage is actually available on this build machine
    def coverageAvailable?
        # Require clang 6.0 to generate coverage output.
        #
        # We can't process coverage data on Ubuntu 16.04 - its LLVM is too old
        # (llvm-cov does not support "export", and it seems to fail to load the
        # merged output for some reason).
        #
        # It's possible this might actually work with some versions of clang
        # between 3.8 and 6.0, but they haven't been tested.
        @clangMajorVersion >= 6
    end

    # Get the toolchain installation path - where other tools like llvm-profdata
    # can be found.  Only used if coverageAvailable?() returns true (the MSVC
    # toolchain does not provide this)
    def toolchainPath
        @toolchainPath
    end

    # If coverage is requested and avaiable, returns opts (used for driver opts
    # and linker opts).  Otherwise, returns [].
    def coverageOpts(requested, opts)
        (requested && coverageAvailable?) ? opts : []
    end

    # Apply moc to a source or header file.  All paths should be absolute paths.
    def moc(sourceFile, outputFile, macros, includeDirs, frameworkPaths)
        params = [@mocPath]
        params += Macros.map { |m| "-D#{m}" }
        params += macros.map { |m| "-D#{m}" }
        params += includeDirs.map { |d| "-I#{d}" }
        # Indicate system include/framework directories explicitly
        params += @mocCppDefIncludeOpts
        params += [
            '-o', outputFile,
            sourceFile
        ]

        sh *params
    end

    # Invoke rcc to compile QRC resources to a source file
    def rcc(sourceFile, outputFile, name)
        params = [
            @rccPath,
            sourceFile,
            '-name', name,
            '-o', outputFile
        ]

        sh *params
    end

    def compileForLang(clang, langCompileOpts, sourceFile, objectFile, depFile,
                       macros, includeDirs, frameworkPaths, coverage)
        # Target type hard-coded for x86_64 below
        raise "Unsupported architecture: #{Build::Architecture}" if Build::Architecture != :x86_64
        params = [clang]
        params += langCompileOpts
        params += [
            '-g',   # Generate debug information
            '-Wall',
            '-Wextra',
            '-pipe',
            '-fvisibility=hidden', # Exports from dynamic libraries are explicitly marked
            '-Wno-unused-parameter',
            '-Wno-dangling-else',
            '-Werror=unused-result',
            '-Werror=return-type',
            '-fPIC',
            '-MMD', # Write dependency file for user headers only
            '-MF', depFile # Specify dependency file
        ]
        params += DriverPlatformOpts[Build::Platform]
        params += CompileOpts[Build::Variant]
        params += coverageOpts(coverage, CoverageDriverOpts)
        params += frameworkPaths.map { |f| "-F#{f}" }
        params += Macros.map { |m| "-D#{m}" }
        params += VariantMacros[Build::Variant].map { |m| "-D#{m}" }
        params += macros.map { |m| "-D#{m}" }
        params += includeDirs.map { |d| "-I#{d}" }
        params += [
            '-o', objectFile,
            '-c', File.absolute_path(sourceFile) # Absolute path allows Qt Creator to open files from diagnostics
        ]

        sh *params
    end

    # Compile one source file to an object file.  All paths should be absolute
    # paths.
    # The :coverage option is used in this step.
    def compile(sourceFile, objectFile, depFile, macros, includeDirs,
                frameworkPaths, options)
        ext = File.extname(sourceFile)
        if(ext == '.c')
            compileForLang(@clang, [], sourceFile, objectFile, depFile, macros,
                           includeDirs, frameworkPaths, options[:coverage])
        elsif(ext == '.cpp' || ext == '.cc')
            compileForLang(@clangpp, CppCompileOpts, sourceFile, objectFile,
                           depFile, macros, includeDirs, frameworkPaths,
                           options[:coverage])
        # Objective-C++ source for macOS
        elsif(ext == '.mm' && Build.macos?)
            compileForLang(@clangpp, ObjCppCompileOpts, sourceFile, objectFile,
                           depFile, macros, includeDirs, frameworkPaths,
                           options[:coverage])
        # Assembling is supported on macOS only to embed plists in text sections
        elsif(ext == '.s' && Build.macos?)
            # There's no way to specify additional assembler include directories
            # currently.
            sh 'as', *DriverPlatformOpts[Build::Platform], '-arch', 'x86_64',
                '-o', objectFile, sourceFile
        else
            raise "Don't know how to compile #{sourceFile}"
        end
    end

    def cppLink(targetFile, objFiles, libPaths, libs, frameworkPaths,
                frameworks, driverArgs, linkerArgs, coverage)
        # Linker script hard-coded for x86_64 on Linux (in LinkPlatformOpts above)
        raise "Unsupported architecture: #{Build::Architecture}" if Build::linux? && Build::Architecture != :x86_64
        linkParams = []
        # Include '$ORIGIN/../lib' in the rpath to find PIA-specific libraries
        # (specifically pia-clientlib) when testing builds from the staging
        # directory.
        linkParams.concat(['-rpath', '$ORIGIN/../lib'])
        # If Qt is used, add the Qt installation to the rpath.  (Otherwise, we
        # don't need to set an rpath.)  During the deploy step, rpaths will be
        # patched to the final installation directory; this rpath is needed for
        # directly running the staged build in development.
        if(libs.any? {|l| l.include?('Qt5Core')} || frameworks.any? {|f| f.include?('QtCore')})
            linkParams.concat(['-rpath', File.join(@qt.qtRoot, 'lib')])
        end
        linkParams.concat(LinkPlatformVariantOpts[Build::Platform][Build::Variant])
        linkParams.concat(LinkPlatformOpts[Build::Platform])
        linkParams.concat(linkerArgs)

        puts "link #{coverage ? "(with code coverage) " : ""}- #{targetFile}"

        params = [
            @clangpp,
            "-Wl,#{linkParams.join(',')}",
        ]
        params += driverArgs
        params += DriverPlatformOpts[Build::Platform]
        params += coverageOpts(coverage, CoverageDriverOpts)
        params += libPaths.map { |l| "-L#{l}" }
        params += frameworkPaths.map { |f| "-F#{f}" }
        params += ['-o', targetFile]
        params += objFiles
        params += coverageOpts(coverage, LinkCoverageStaticLibBeginOpts[Build::Platform])
        params += libs.map { |l| l.include?('/') ? l : "-l#{l}" }
        params += coverageOpts(coverage, LinkCoverageStaticLibEndOpts[Build::Platform])
        params += frameworks.flat_map { |f| ['-framework', f] }

        sh *params

        # Separate symbols from binaries on Mac
        if(Build.macos?)
            sh 'dsymutil', '-o', "#{targetFile}.dSYM", targetFile
            sh 'strip', '-S', targetFile
        end
    end

    def linkStaticLib(target, objects)
        sh 'ar', 'rcs', target, *objects if Build.linux?
        # macOS needs the TOC to be built when creating or updating the lib,
        # use libtool.
        sh 'libtool', '-c', '-static', '-o', target, *objects
    end

    # Link a target.
    # The :type and :coverage options are used in this step
    def link(targetFile, objFiles, libPaths, libs, frameworkPaths, frameworks,
             extraArgs, options)
        if(options[:type] == :dynamic)
            dynamicLinkOpts = []
            dynamicDriverOpts = []

            if(Build.linux?)
                dynamicLinkOpts = ["-soname=#{File.basename(targetFile)}", '--as-needed']
                dynamicDriverOpts = ['-shared']
            elsif(Build.macos?)
                dynamicLinkOpts = ['-install_name',
                                   "@executable_path/#{File.basename(targetFile)}"]
                dynamicDriverOpts = ['-dynamiclib', '-current_version', Build::VersionBase]
            end

            cppLink(targetFile, objFiles, libPaths, libs, frameworkPaths,
                    frameworks, dynamicDriverOpts, dynamicLinkOpts + extraArgs,
                    options[:coverage])
        elsif(options[:type] == :executable)
            cppLink(targetFile, objFiles, libPaths, libs, frameworkPaths,
                    frameworks, [], extraArgs, options[:coverage])
        elsif(options[:type] == :static)
            sh 'ar', 'rcs', targetFile, *objFiles
        end
    end
end
