require_relative 'model/build.rb'
require_relative 'util/util.rb'
require_relative 'util/dsl.rb'

# ExecutableBuilder compiles and links an executable from source files, using
# the files and settings specified to Executable.  ExecutableBuilder handles a
# single target architecture.  When building for a universal target, Executable
# then combines all of the single-architecture outputs.
#
# ExecutableBuilder isn't meant to be used directly; Executable provides the
# interface to add source files and specify the build configuration.
class ExecutableBuilder

    include BuildDSL

    # Executable provides:
    # - name - the target name
    # - target - where we should place the built executable
    # - The configuration data, which are set up by Executable and then shared
    #   with each ExecutableBuilder.  Note that these are not fully set up when
    #   initialize() is called, but they will be when the build tasks are
    #   executed.
    def initialize(name, tc, architecture, target, macros,
        includeDirs, libPaths, libs, frameworkPaths, frameworks, linkArgs,
        options)
        @name = name
        @tc = tc
        @architecture = architecture
        @target = target

        @macros = macros
        @includeDirs = includeDirs
        @libPaths = libPaths
        @libs = libs
        @frameworkPaths = frameworkPaths
        @frameworks = frameworks
        @linkArgs = linkArgs
        @options = options

        # Link task - collects inputs from dependencies
        # Multifile - allows object compilation to occur in parallel
        multifile @target => [] do |t|
            objs = t.prerequisites.select { |pr| @tc.isObjectFile?(pr) }
            plistObj = generateDefaultInfoPlist
            if(plistObj != nil)
                objs << plistObj
            end

            link(t.name, objs)
        end
    end

    private

    # Link the executable
    def link(name, objs)
        @tc.link(@architecture, name, objs, @libPaths, @libs, @frameworkPaths,
                 @frameworks, @linkArgs, @options)
    end

    # Generate a default Info.plist if applicable to this executable.  The plist
    # is generated and compiled, the .o file path is returned (or nil if no
    # default plist is needed)
    def generateDefaultInfoPlist()
        # Only needed for Mac console apps that don't have an explicit
        # Info.plist input
        return nil if !Build.macos? ||
            @options[:type] != :executable ||
            @options[:interface] != :console ||
            @options[:autoInfoPlist] == false

        infoPlistFile = @target + '.Info.plist'
        File.write(infoPlistFile, MacInfoPlist.renderMacosAppPlist(@name, @name, Build::ProductIdentifier))

        plistSContent = ".section __TEXT,__info_plist\n" +
            ".incbin \"#{File.absolute_path(infoPlistFile)}\"\n"
        plistSFile = @target + '.Info.plist.s'
        File.write(plistSFile, plistSContent)

        plistObjPath = plistSFile + @tc.objectExt(plistSFile)
        @tc.compile(@architecture, plistSFile, plistObjPath, nil, @macros,
                   @includeDirs, @frameworkPaths, @options)
        plistObjPath
    end

    public

    def architecture
        @architecture
    end

    def target
        @target
    end

    # Run moc on a header file, then compile the result and include it in the
    # object.
    #
    # For both header and source files, Executable decided whether it needs moc
    # and tells us where to put the output - that decision doesn't depend on
    # architecture.
    #
    # The actual moc and compilation is done per-architecture.  It's probably
    # not likely that 'moc' actually depends on arch, but it doesn't take long,
    # and this avoids an assumption that include/framework paths obtained from
    # clang -E -v (which does require a target) don't actually depend on the
    # target architecture.
    def mocHeader(headerPath, mocSrcPath, mocObjPath)
        directory File.dirname(mocObjPath)
        file mocSrcPath => [headerPath, File.dirname(mocObjPath)] do |t|
            puts "moc #{@name}: #{headerPath}"
            @tc.moc(@architecture, headerPath, mocSrcPath, @macros, @includeDirs,
                    @frameworkPaths)
        end
        # Compile the moc source and include it in the program
        compileSource(mocSrcPath, mocObjPath, nil)
    end

    # Compile a source file to an object file, and include that object in the
    # executable.
    #
    # Normally, sourceFile() picks an object path using the source path.
    # resource() also uses this directly; the source file is generated so
    # sourceFile() wouldn't be able to select an object path.
    #
    # A dependency file will be generated alongside the object file to list the
    # header dependencies of that object file.  Subsequent builds reload these
    # dependencies, so Rake knows to rebuild an object even if only header files
    # changed.
    #
    # TODO - moc is still run as part of the compilation step, Executable should
    # run this once per source file.  Executable also has the needsMoc?
    # implementation
    def compileSource(sourcePath, objPath, mocSrcPath)
        objBuildDir = File.dirname(objPath)
        directory objBuildDir

        # .mf extension is required so import() knows to use the Makefile parser
        depFile = objPath.ext(".dep.mf")

        file objPath => [sourcePath, objBuildDir] do |t|
            puts "compile #{@name}: #{sourcePath}"

            # If the file needs moc, run it.  However, we can't compile the moc
            # source separately, since we can't include the class definitions
            # ahead of the moc source - the source file has to #include the moc
            # source at the end of the file.
            #
            # We don't need an explicit task for this, it's just part of
            # transforming this source file to an object file.
            fileIncludeDirs = @includeDirs
            if mocSrcPath != nil
                @tc.moc(@architecture, sourcePath, mocSrcPath, @macros, @includeDirs,
                       @frameworkPaths)
                # Add this directory to the include paths so the source file can
                # include the moc source
                fileIncludeDirs = fileIncludeDirs + [File.dirname(mocSrcPath)]
            end

            @tc.compile(@architecture, sourcePath, objPath, depFile, @macros,
                       fileIncludeDirs, @frameworkPaths, @options)
        end

        # If a dep file exists from a prior run, import it so we can create
        # dependencies on the header files that were used last time.
        if(File.exist?(depFile))
            # Rake is able to import Makefile dependencies, but in this case
            # it's relatively common that a dependency no longer exists (such as
            # when switching branches where a header no longer exists).  Load
            # this manually and ignore any files that don't exist.
            deps = File.read(depFile)
            # This parser is simpler than the built-in makefile loader since it
            # is only used with generated makedep files.
            target = nil
            deps.each_line do |l|
                if(target == nil)
                    target, tail = l.split(':', 2)
                    # tail ignored; it theoretically could contain deps but the
                    # generated makedep files are never written that way - it's
                    # always just ' \'
                else
                    # Remove leading/trailing spaces, final newline, and any trailing '\'
                    l.gsub!(/^[ \t]+/, '')
                    l.gsub!(/[ \t]+(|\\)$/, '')
                    l = Util.deleteSuffix(l, "\n")
                    # Unescape embedded spaces
                    l.gsub!('\\ ', ' ')
                    if(File.exist?(l))
                        task objPath => l
                    end
                end
            end
        end

        # Include object in executable
        task @target => objPath
        self
    end
end
