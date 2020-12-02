require_relative 'toolchain/msvc.rb' if Build.windows?
require_relative 'toolchain/clang.rb' unless Build.windows?
require_relative 'model/build.rb'
require_relative 'model/component.rb'
require_relative 'model/qt.rb'
require_relative 'util/util.rb'
require_relative 'util/dsl.rb'
require_relative 'macinfoplist.rb'
require 'rake/loaders/makefile'

# Build an executable image from any number of source directories.  The result
# can be an executable or a dynamic/shared library.  ("Module" might be a more
# appropriate term than "Executable", but Ruby already has a class called
# "Module".)
#
# Source directories are added using includeSource():
#   Executable.new(:say_hello, :executable)
#      .source('say_hello/src')  # includes 'say_hello/src/main.cpp', etc.
#
# Libraries, macros, include directories, etc. can all be specified similarly.
# The order is not important, all defined macros/include dirs/etc. will be used
# to compile all objects in the module, regardless of the order specified.
# For example:
#   Executable.new(:international_hello, :executable)
#      .include('libsayhello/src')              # path in project
#      .include('/opt/libtranslate/include')    # absolute path
#      .define('DEFAULT_HELLO=howdy')           # define a macro
#      .source('international_hello/src')       # add sources
#
# export() provides a component definition for use by other components.
# For example:
#   speakLib = Executable.new(:speak_lib, :dynamic)
#       ... # sources, etc.
#       .define('DYNAMIC_SPEAK_LIB') # export a macro
#   sayHello = Executable.new(:say_hello, :executable)
#       ... # sources, etc.
#       .use(speakLib.export) # use the speak_lib dynamic library
#   task default: sayHello.target # builds say_hello executable and dependencies
#
# See export() and use() for more details.
class Executable
    # Find Qt
    Qt = Qt.new

    # Select and initialize a toolchain.
    #
    # Executable uses a toolchain driver to compile and link targets.  Each
    # toolchain provides equivalent methods to compile an object (with macros
    # and include dirs) and link targets (with lib paths and libs).
    #
    # There are a few specific options passed to the toolchain using the
    # "options" hash:
    #
    # :type => :static, :dynamic, or :executable
    #  - Determines the type module produced by the link step.  Use targetExt()
    #    to get the appropriate extension.  Default is :executable.
    # :runtime -> :static, :dynamic
    #  - How to link the runtime libraries on Windows - use either the static or
    #    dynamic runtime library.  MSVC only.  Default is :dynamic.
    # :interface -> :gui, :console
    #  - Determines whether the target requires a console interface (sets the
    #    subsystem in the linked module).  MSVC only.  Default is :console.
    # :coverage -> true, false
    #  - Enable code coverage measurement - intended for unit tests.  Clang
    #    only, ignored for clang <6.0.  Default is false.
    Tc = Build.windows? ? MsvcToolchain.new(Qt) : ClangToolchain.new(Qt)

    # Determine the source subdirectories to use to pick up platform-specific
    # source.  Include '' for non-platform-specific source.
    def self.calcSourceSubdirs
        [''].tap do |v|
            v << 'win' if Build.windows?
            v << 'mac' if Build.macos?
            v << 'linux' if Build.linux?
            v << 'posix' if Build.posix?
        end
    end
    SourceSubdirs = calcSourceSubdirs

    # Glob patterns always excluded from resource directories
    ResourceExcludePatterns = ['**/.DS_Store', '**/*.autosave']

    include BuildDSL

    # Create an Executable:
    # - name - Name of the final module, excluding any extension (an extension
    #   will be added if needed based on platform and module type)
    # - Type - The type of module to build, controls the final link step.
    #   Defaults to :executable
    #   :executable - The module will be an executable ('.exe', no ext on Unix)
    #   :dynamic - The module will be a dynamic library ('.dll'/'.so')
    #   :static - The module will be a static library ('.lib'/'.a')
    def initialize(name, type = :executable)
        @name = name
        @build = Build.new(name)
        @macros = []
        @includeDirs = []
        @libPaths = []
        @libs = []
        @frameworkPaths = []
        @frameworks = []
        @linkArgs = []
        @options = {
            type: type,
            runtime: :dynamic,
            interface: :console,
            coverage: false
        }

        # Whether we have referenced QtCore yet; it's added the first time we
        # reference a Qt module
        @hasQtCore = false

        # Suppress the default Info.plist for Mac console executables
        @noDefaultInfoPlist = false

        # All the resources collected for the QRC task.  Nil if no resources
        # have been defined yet, this indicates that we need to create the QRC
        # task the first time a resource if given.
        #
        # Once initialized, this lists all the resource files in their root
        # directories.  It's an array of objects of the form:
        # {
        #   root: <a root given to resource()>
        #   files: <array of file paths>
        # }
        @qrcResources = nil
        # Where the QRC XML file will be generated (if resources are given)
        @qrcXml = @build.artifact("qrc_#{name}.qrc")

        # Link task - collects inputs from dependencies
        moduleFile = name + Tc.targetExt(type)
        @moduleTarget = @build.artifact(moduleFile)
        # Multifile - allows object compilation to occur in parallel
        multifile @moduleTarget => [@build.componentDir] do |t|
            objs = t.prerequisites.select { |pr| Tc.isObjectFile?(pr) }
            plistObj = generateDefaultInfoPlist
            if(plistObj != nil)
                objs << plistObj
            end
            Tc.link(t.name, objs, @libPaths, @libs, @frameworkPaths,
                    @frameworks, @linkArgs, @options)
        end

        # Create symlinks to the target if needed
        @symlinkTargets = Tc.symlinkExts(type).map{|e| @build.artifact("#{name}#{e}")}
        @symlinkTargets.each do |l|
            file l => @build.componentDir do
                if(File.exist?(l))
                    FileUtils.rm(l)
                end
                FileUtils.ln_sf(moduleFile, l)
            end
        end

        @exportComponent = Component.new(@moduleTarget)
            .libPath(@build.componentDir)
        @exportComponent.lib(name) if Build.windows?
        @exportComponent.lib(@moduleTarget) if Build.posix?
    end

    private

    # Does an input file need moc?
    def needsMoc?(filePath)
        # Ignore Windows resource scripts, they're in UTF-16 and don't need to
        # be moc'd
        if(File.extname(filePath) == '.rc')
            return false
        end
        # Qbs uses a proper C++ lexer to look for Q_OBJECT tokens.  This just
        # does a regex search, which could be fooled by Q_OBJECT occurring in
        # a string literal or comment, but at worst we'd just run moc
        # unnecessarily on those files, and having Q_OBJECT in a string or
        # comment is unusual.
        #
        # Qbs also has some additional logic for Q_PLUGIN_METADATA, but we're
        # not using that.
        mocPattern = /\bQ_(OBJECT|GADGET|NAMESPACE)\b/
        File.open(filePath) do |srcFile|
            srcFile.each_line.any? do |line|
                line.include?('Q_OBJECT') || line.include?('Q_GADGET') || line.include?('Q_NAMESPACE')
            end
        end
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
            @noDefaultInfoPlist

        infoPlistFile = @build.artifact('Info.plist')
        File.write(infoPlistFile, MacInfoPlist.renderDefaultPlist(@name, @name, Build::ProductIdentifier))

        plistSContent = ".section __TEXT,__info_plist\n" +
            ".incbin \"#{File.absolute_path(infoPlistFile)}\"\n"
        plistSFile = @build.artifact('Info.plist.s')
        File.write(plistSFile, plistSContent)

        plistObjPath = @build.artifact(File.basename(plistSFile) + Tc.objectExt(plistSFile))
        Tc.compile(plistSFile, plistObjPath, nil, @macros, @includeDirs,
                   @frameworkPaths, @options)
        plistObjPath
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
    def compileSource(sourcePath, objPath)
        # Build intermediate directory
        # Ex: .../pia-clientlib + src/win
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
            if needsMoc?(sourcePath)
                mocSrcPath = @build.artifact(sourcePath.ext(".moc"))
                Tc.moc(sourcePath, mocSrcPath, @macros, @includeDirs,
                       @frameworkPaths)
                # Add this directory to the include paths so the source file can
                # include the moc source
                fileIncludeDirs = fileIncludeDirs + [File.dirname(mocSrcPath)]
            end

            Tc.compile(sourcePath, objPath, depFile, @macros, fileIncludeDirs,
                       @frameworkPaths, @options)
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
        task @moduleTarget => objPath
        self
    end

    public

    # Get the path to the built module - can be used to add a dependency on the
    # module task.  For installation use install() to also install symlinks to
    # .so/.dylib modules.
    def target
        @moduleTarget
    end

    # Install all artifacts to an install target.  targetPath can be a symbol
    # understood by Install.install() or a path relative to the install root
    #
    # This picks up symbolic link targets when applicable in addition to the
    # module target.
    def install(installation, targetPath)
        installation.install(@moduleTarget, targetPath)
        @symlinkTargets.each {|t| installation.install(t, targetPath)}
        self
    end

    # Specify whether to link the runtime library statically or dynamically.
    # The default is :dynamic, use :static to link statically.  Only used on
    # Windows.
    def runtime(runtime)
        @options[:runtime] = runtime
        self
    end

    # Indicate that this is a GUI program on Windows and Mac.
    # Windows - sets the subsystem to WINDOWS to indicate that it doesn't need a
    # console interface.
    # Mac - does not embed an Info.plist.
    def gui
        @options[:interface] = :gui
        self
    end

    # Suppress the default Info.plist on Mac for console executables.  Use if
    # the executable links in its own customized Info.plist.
    def noDefaultInfoPlist
        @noDefaultInfoPlist = true
        self
    end

    # Enable code coverage tracking for this module, if supported by the
    # toolchain
    def coverage(enable)
        @options[:coverage] = enable
        self
    end

    # Define a macro in this module.  Can be just a macro name to define an
    # empty macro (mod.define('UNICODE'), etc.), or name=value to define a macro
    # with a value (mod.define('VERSION=2.0.1'), etc.)
    #
    # With visibility = :export, the macro is also exported to other modules
    # that use this one.
    def define(macro, visibility = nil)
        if(visibility == :export)
            @exportComponent.define(macro)
        end
        @macros.push(macro)
        self
    end

    # Define an include directory in this module.
    def include(dir, visibility = nil)
        if(visibility == :export)
            @exportComponent.include(dir)
        end
        @includeDirs.push(File.absolute_path(dir))
        self
    end

    # Define a library path to be searched for the specified libraries
    def libPath(dir)
        @libPaths.push(File.absolute_path(dir))
        self
    end

    # Specify the name of a library to be linked into this target.
    # On Windows, this will be passed to the linker as a static library name
    # (.lib is added).  On Unix, this will be passed as a -lname option (the
    # linker interprets this as libname.a).
    def lib(name)
        @libs.push(name)
        self
    end

    # Add framework paths and frameworks for macOS
    def frameworkPath(dir)
        @frameworkPaths.push(File.absolute_path(dir))
        self
    end
    def framework(name)
        @frameworks.push(name)
        self
    end

    # Add platform-specific linker arguments
    def linkArgs(args)
        @linkArgs += args
        self
    end

    # Use another component in this one by adding its exported macros, include
    # directories, and libraries.  The module can be one built by this program
    # or a module found on the system, such as a Qt module.
    #
    # For example:
    # - To build a dynamic module and use it from another module:
    #   speak = Executable.new('speak', :dynamiclib)
    #       ... # sources, etc.
    #       .define('SPEAK_LIB', :export)
    #   reader = Executable.new('reader', :executable)
    #       ... # sources, etc.
    #       .use(speak.export) # reference speak module, also defines SPEAK_LIB
    #
    # - To use a module found on the system
    #   qt = Qt.new(...) # find Qt
    #   downloader = Executable.new('downloader', :executable)
    #       ... # sources, etc.
    #       .use(Qt.find('Core'))
    #       .use(Qt.find('Network'))
    #
    # See export() for details on the parts of an Executable that are exported to
    # other components, and for the required keys on module definitions.
    def use(component)
        @macros.concat(component.macros)
        @includeDirs.concat(component.includeDirs)
        @libPaths.concat(component.libPaths)
        @libs.concat(component.libs)
        @frameworkPaths.concat(component.frameworkPaths)
        @frameworks.concat(component.frameworks)

        if(component.task != nil)
            task @moduleTarget => component.task
        end
        self
    end

    # Use a Qt module, such as "Network" or "Xml".
    # "Core" is referenced automatically when the first Qt module is referenced.
    # Don't pass "Core" explicitly since it handles essential Qt definitions.
    # To only use "Core" without using any other Qt modules, use '.useQt(nil)'.
    def useQt(moduleName)
        if(!@hasQtCore)
            use(Qt.core)
            @hasQtCore = true
        end
        if(moduleName != nil)
            use(Qt.component(moduleName))
        end
        self
    end

    # Export a component definition of this executable for use by other
    # components.  Generally only makes sense for static or dynamic libraries.
    #
    # Component definitions contain the following keys (always, although array
    # values may be empty):
    #
    # - :target - Name of the rake task that builds the module's target (used
    #   to create a dependency)
    # - :macros - Array of exported macro definitions, see define()
    # - :includeDirs - Array of absolute paths to exported include directories
    # - :libPaths - Array of absolute paths to search for libs
    # - :libs - Array of library names to link into the module
    #
    # Executable exports the following:
    # - :target - the name of this module's target task
    # - :macros - exported macros defined with .define(..., :export)
    # - :includeDirs - source directories (exported as include directories by
    #   default, see source()), and exported include directories defined with
    #   .includeDir(..., :export)
    # - :libPaths - the module's build path, which contains the import library
    #   or static library
    # - :libs - the target name (refers to the static library or import library
    #   built for the module)
    #
    # No framework paths or frameworks are exported on macOS.
    def export()
        @exportComponent
    end

    # Include a directory tree as QRC resources.
    #
    # The file pattern is given in two parts - a root directory, and one or more
    # globs (or exact paths) identifying the files to include.  The path
    # relative to the root directory is used in the QRC file.
    #
    # For example, with root 'client/res' and include 'img/**/*.png', the build
    # might find 'client/res/img/dashboard/splash.png'.  This will be placed in
    # the resource file as 'img/dashboard/splash.ing' - the root of 'client/res'
    # is excluded.
    #
    # 'excludePatterns' can be used to ignore files that would otherwise match
    # 'patterns'.  A few patterns are always excluded (ResourceExcludePatterns)
    def resource(root, patterns, excludePatterns = [])
        # If we don't have a QRC source task yet, create one
        qrcSourcePath = @build.artifact("qrc_#{@name}.cpp")
        if(@qrcResources == nil)
            @qrcResources = []
            # Generate a QRC resource script (as XML)
            file @qrcXml => [@build.componentDir] do |t|
                File.open(t.name, "w") do |f|
                    f.write("<!DOCTYPE RCC><RCC version=\"1.0\"><qresource>\n")
                    @qrcResources.each do |group|
                        group[:files].each do |res|
                            effectivePath = Util.deletePrefix(res, group[:root])
                            f.write("<file alias=\"#{effectivePath}\">#{File.absolute_path(res)}</file>\n")
                        end
                    end
                    f.write("</qresource></RCC>\n")
                end
            end
            # Use rcc to generate a source file
            file qrcSourcePath => [@qrcXml] do |t|
                puts "rcc #{@name}"
                Tc.rcc(@qrcXml, t.name, @name)
            end
            # Include it in the build
            qrcObj = @build.artifact(File.basename(qrcSourcePath) + Tc.objectExt(qrcSourcePath))
            compileSource(qrcSourcePath, qrcObj)
        end

        resourcePatterns = Util.joinPaths([[root], patterns])
        resources = Rake::FileList[resourcePatterns]
            .exclude(*Util.joinPaths([[root], ResourceExcludePatterns]))
            .exclude(*Util.joinPaths([[root], excludePatterns]))
            .select {|f| !File.directory?(f)}
        @qrcResources << {root: root, files: resources}
        # qrcXml depends on all of these files
        task @qrcXml => resources

        self
    end

    # Include a specific source file in the object.
    #
    # Most of the time, use .source(path) to specify a whole directory instead;
    # specific files are used for deps like Breakpad when the whole directory is
    # not needed.
    def sourceFile(sourcePath)
        # Ex: .../pia-clientlib + src/win/win_com.cpp.obj
        objPath = @build.artifact(sourcePath + Tc.objectExt(sourcePath))
        compileSource(sourcePath, objPath)
    end

    # Include a specific header file in the object.  Header files obviously
    # aren't compiled themselves, this just checks for a possible moc task.
    #
    # Most of the time, use .source(path) to specify a whole directory instead.
    def headerFile(headerPath)
        if needsMoc?(headerPath)
            mocSrcPath = @build.artifact("#{headerPath}.moc.cpp")
            # Build intermediate directory - includes subdirectory path of
            # source file
            objBuildDir = @build.artifact(File.dirname(headerPath))
            directory objBuildDir

            # Generate source with moc
            file mocSrcPath => [headerPath, objBuildDir] do |t|
                puts "moc #{@name}: #{headerPath}"
                Tc.moc(headerPath, mocSrcPath, @macros, @includeDirs,
                       @frameworkPaths)
            end

            # Compile the moc src and link it into the program
            mocObjPath = mocSrcPath + Tc.objectExt(mocSrcPath)
            compileSource(mocSrcPath, mocObjPath)

            # Include object in executable
            task @moduleTarget => mocObjPath
        end
        self
    end

    # Include multiple source files - just calls sourceFile() on each.
    # Prefer source() with a directory path except when specifically including
    # only some files from a directory.
    def sourceFiles(paths)
        paths.each {|p| sourceFile(p)}
        self
    end

    # Include the source files from this directory, relative to the project
    # root.
    #
    # By default, source directories are exported as include directories to
    # other modules that use this one.  Pass nil for visibility to prevent
    # this.
    #
    # - Platform-specific subdirectories (win, mac, linux, posix), are included
    #   automatically
    # - moc rules are generated for headers in this directory, the resulting
    #   files are compiled and linked into the module
    # - The source directory is also added as an include directory
    def source(path, includeVisibility = :export)
        include(path, includeVisibility)

        # For headers with moc macros, generate moc and compile rules
        headerPatterns = Util.joinPaths([[path], SourceSubdirs, ["*.h"]])
        Rake::FileList[headerPatterns].each do |headerPath|
            headerFile(headerPath)
        end

        # This intentionally includes all platform-specific extensions
        # regardless of platform - .rc files shouldn't show up in non-Windows
        # builds, etc.
        srcPatterns = ["*.cpp", "*.cc", "*.c", "*.mm", "*.rc", "*.s"]
        dirPatterns = Util.joinPaths([[path], SourceSubdirs, srcPatterns])

        Rake::FileList[dirPatterns].each do |sourcePath|
            sourceFile(sourcePath)
        end

        self
    end
end
