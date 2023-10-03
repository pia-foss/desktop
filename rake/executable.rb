require_relative 'toolchain/msvc.rb' if Build.windows?
require_relative 'toolchain/clang.rb' unless Build.windows?
require_relative 'model/build.rb'
require_relative 'model/component.rb'
require_relative 'model/qt.rb' if Build.desktop?
require_relative 'util/util.rb'
require_relative 'util/dsl.rb'
require_relative 'macinfoplist.rb'
require_relative 'executablebuilder.rb'
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
    # Determine the architectures we're building for - one for a specific
    # arch, multiple for a universal build on XNU targets
    Architectures = Build::TargetArchitecture == :universal ?
        Build::PlatformUniversalArchitectures[Build::Platform] :
        [Build::TargetArchitecture]

    # Find Qt when targeting Desktop platforms only
    Qt = Build.desktop? ? Qt.new : nil

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
    Tc = Build.windows? ? MsvcToolchain.new(Build::TargetArchitecture, Qt) :
        ClangToolchain.new(Architectures, Qt)

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

        # Hack for Windows - limit component directory name to 30 chars.
        # The PIA installer executable name can be pretty long for feature
        # branches when there's also a prerelease tag, etc.  This itself is
        # fine, but also including that in the component directory name often
        # exceeds the Windows 260-char path limit.
        #
        # Chopping the directory name on Windows is fine, this just assumes that
        # there won't be two built components in the same build with matching
        # 30-char prefixes, which is reasonable.
        buildName = name
        if Build.windows?
            buildName = name[0, 30]
            # Avoid ending in '.' - this isn't a valid directory name in
            # Windows.  It seems to just ignore it, but avoid it anyway for
            # robustness.
            buildName.chomp!('.') while buildName.end_with?('.')
        end
        @build = Build.new(buildName)

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
            coverage: false,
            forceLinkSymbols: [],
            autoInfoPlist: true
        }

        # Whether we have referenced QtCore yet; it's added the first time we
        # reference a Qt module
        @hasQtCore = false

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

        # This is the final target the Executable produces
        moduleFile = name + Tc.targetExt(type)
        @moduleTarget = @build.artifact(moduleFile)

        # The task structuring here is a bit complex due to a limitation in
        # Rake.  Whenever the dependencies of a parallel task (multitask or our
        # "multifile") converge, more than one thread can try to invoke the
        # same dependency, and threads after the first will just block.
        #
        # exe(uni.) ---> exe(x86_64) ---> lib(uni.) ---> [...]
        #            '-> exe(arm64) --'
        #
        # If exe(uni.) is a parallel task, this will waste one thread during the
        # build of "lib" due to both threads trying to execute lib(uni.) at the
        # same time (the second just blocks).
        #
        # Convergence can happen a lot in Executable due to the large number of
        # shared libraries that we now have.  To work around this, restructure
        # the tasks, so that the flow from exe(uni.) through to lib(uni.) is
        # _not_ parallel:
        #
        # exe(uni.) ---> .deps ---> lib(uni.) ---> [...]
        #            '
        #            '-> .arches ---> exe(x86_64)
        #                         '-> exe(arm64)
        #
        # exe(uni.) and .deps are serial tasks, but .arches is parallel.
        # This preserves most of the properties we want:
        #  * we don't waste a thread while building "lib"
        #  * the arch executables can still be built in parallel
        #  * the arch executables' objects can still be compiled in parallel
        #  * the deps are always built before arch executables (due to the
        #    non-parallel task for exe(uni.) building in a known order)
        #
        # We lose some parallelism when multiple dependencies could be built in
        # parallel (.deps is serial).  In many cases, the deps are actually
        # independent, but we don't know here whether they could converge
        # Still, we get most of the benefit just by compiling objects in
        # parallel within an executable.
        #
        # Though not represented here, we do still put a dependency from
        # exe(x86_64) and exe(arm64) on .deps since they do require it.  This
        # has almost no effect since .deps would be built already by the serial
        # exe(uni.) task, but `rake -m` can force all tasks to be parallel, so
        # we must declare the proper dependency too.  (-m would waste a lot of
        # threads due to convergence, but at least it would work.)

        # The dummy tasks still need files so Rake knows when to re-run them
        # using the dummy file's mtime.  (Non-file tasks always run, we don't
        # want that.)
        @dependencyTarget = @build.artifact(".deps")
        # This task depends on nothing right now; dependencies added with .use()
        # will be attached here.
        file @dependencyTarget => @build.componentDir do |t|
            FileUtils.touch(t.name)
        end

        @builders = nil
        if(Architectures.length == 1)
            # Create a single builder, with no architecture subdirectory in the
            # build directory, producing the final target
            @builders = [ExecutableBuilder.new(name, Tc, Architectures[0],
                @moduleTarget, @macros, @includeDirs, @libPaths, @libs,
                @frameworkPaths, @frameworks, @linkArgs, @options)]
            # Although @moduleTarget is a multitask, this doesn't cause a
            # convergence problem as the dependencies are grouped into a serial
            # task.  The other dependencies of @moduleTarget are individual
            # object compilations that will not converge.
            task @moduleTarget => [@build.componentDir, @dependencyTarget]
        else
            # Create a builder for each architecture
            @builders = Architectures.map do |a|
                # Create a build subdirectory for this arch
                archBuild = @build.artifact(a.to_s)
                directory archBuild => [@build.componentDir]
                # The arch target is in the subdirectory
                archTarget = @build.artifact(File.join(a.to_s, moduleFile))
                # Create the builder for this arch
                builder = ExecutableBuilder.new(name, Tc, a, archTarget,
                    @macros, @includeDirs, @libPaths, @libs, @frameworkPaths,
                    @frameworks, @linkArgs, @options)
                # The builder's output depends on the builder's output dir
                task archTarget => [archBuild, @dependencyTarget]
                builder
            end
            # Create a 'lipo' task to combine each of the architecture builds.
            builderTargets = @builders.map { |b| b.target }
            archesTarget = @build.artifact(".arches")
            multifile archesTarget => builderTargets do |t|
                FileUtils.touch(t.name)
            end
            file @moduleTarget => [@dependencyTarget, archesTarget] do |t|
                FileUtils.rm(@moduleTarget) if File.exist?(@moduleTarget)
                Util.shellRun 'lipo', '-create', *builderTargets, '-output', @moduleTarget
                # Combine the dSYM bundles too
                if(type != :static)
                    FileUtils.rm_rf(@moduleTarget + ".dSYM")
                    FileUtils.mkdir_p(@moduleTarget + ".dSYM/Contents/Resources/DWARF")
                    FileUtils.cp(@builders[0].target + ".dSYM/Contents/Info.plist",
                                 @moduleTarget + ".dSYM/Contents/")
                    builderSyms = builderTargets.map do |t|
                        t + ".dSYM/Contents/Resources/DWARF/" + File.basename(t)
                    end
                    Util.shellRun 'lipo', '-create', *builderSyms, '-output',
                        @moduleTarget + ".dSYM/Contents/Resources/DWARF/" + File.basename(@moduleTarget)
                end
            end
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
        # Nothing needs moc when not using Qt
        if(Qt == nil)
            return false
        end
        # Ignore Windows resource scripts, they're in UTF-16 and don't need to
        # be moc'd
        if(File.extname(filePath) == '.rc')
            return false
        end
        # Qbs uses a proper C++ lexer to look for Q_OBJECT tokens.  This just
        # does a text search, which could be fooled by Q_OBJECT occurring in
        # a string literal or comment, but at worst we'd just run moc
        # unnecessarily on those files, and having Q_OBJECT in a string or
        # comment is unusual.
        #
        # Qbs also has some additional logic for Q_PLUGIN_METADATA, but we're
        # not using that.
        File.open(filePath) do |srcFile|
            srcFile.each_line.any? do |line|
                line.include?('Q_OBJECT') || line.include?('Q_GADGET') || line.include?('Q_NAMESPACE')
            end
        end
    end

    # Get the absolute path to an artifact to be built by a specific builder.
    # Includes the builder's architecture only if more than one arch is being
    # built.
    def archArtifact(path, builder)
        return @build.artifact(path) if @builders.length == 1
        @build.artifact(File.join(builder.architecture.to_s, path))
    end

    # Compile a source file to an object file, and include that object in the
    # executable.  'objPath' is an object file path relative to the per-arch
    # build directory.
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
        # Check if the file needs moc.  This happens at probe time, so any
        # generated files may not exist yet.  We don't need moc on generated
        # files anyway.
        mocSrcPath = (File.exist?(sourcePath) && needsMoc?(sourcePath)) ?
            sourcePath.ext(".moc") : nil
        @builders.each do |b|
            archObjPath = archArtifact(objPath, b)
            archMocSrcPath = (mocSrcPath == nil) ? nil : archArtifact(mocSrcPath, b)
            b.compileSource(sourcePath, archObjPath, archMocSrcPath)
        end

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
        @options[:autoInfoPlist] = false
        self
    end

    # Enable code coverage tracking for this module, if supported by the
    # toolchain
    def coverage(enable)
        @options[:coverage] = enable
        self
    end

    # Force the linker to think a particular symbols is unreferenced, so the
    # object defining that symbol will be linked in from a static library even
    # if it is otherwise unreferenced.
    #
    # This is needed for the unit test static lib to ensure that some static
    # initializers run correctly.  The symbol must be a C-linkage function (the
    # name is mangled on Win x86 assuming that it is a __cdecl function).
    def forceLinkSymbol(symbol)
        @options[:forceLinkSymbols].push(symbol)
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
        @linkArgs.push(*args)
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
    def use(component, visibility = nil)
        @macros.concat(component.macros)
        @includeDirs.concat(component.includeDirs)
        @libPaths.concat(component.libPaths)
        @libs.concat(component.libs)
        if(visibility == :export)
            @exportComponent.dependency(component)
        end
        @frameworkPaths.concat(component.frameworkPaths)
        @frameworks.concat(component.frameworks)

        if(component.task != nil)
            task @dependencyTarget => component.task
        end

        # Recurse into the component's dependencies.  This must be last, because
        # the order of static libraries in @libs is important.  (If a static lib
        # references another static lib, the "dependent" lib must be first, so
        # that the unknown symbols can then be resolved with the "dependency"
        # lib.  This can happen with unit tests, as all-tests-lib.a depends on
        # other static libraries, although code coverage may hide this problem
        # as it causes --whole-archive to be enabled when possible.)
        component.dependencies.each do |dep|
            # Do not export the dependencies even if exporting this component as
            # a dependency.  If we export this component, its dependencies are
            # picked up transitively when used.
            use(dep, nil)
        end
        self
    end

    # Use a Qt module, such as "Network" or "Xml".
    # "Core" is referenced automatically when the first Qt module is referenced.
    # Don't pass "Core" explicitly since it handles essential Qt definitions.
    # To only use "Core" without using any other Qt modules, use '.useQt(nil)'.
    def useQt(moduleName, visibility = nil)
        # Can't use Qt modules on platforms where we do not use Qt
        if(Qt == nil)
            raise "Qt#{moduleName} not available - PIA does not use Qt on #{Build::Platform}"
        end
        if(!@hasQtCore)
            # In principle, we should probably only export Qt.core if at least
            # one other Qt component is exported, or if the executable
            # explicitly said to export core with '.useQt(nil, :export)'.
            #
            # In practice, all of our Qt-based modules need to export Qt.core,
            # and there's virtually no harm exporting it even if not needed.
            # So there's not much point actually adding that logic; just export
            # it.
            use(Qt.core, :export)
            @hasQtCore = true
        end
        if(moduleName != nil)
            use(Qt.component(moduleName), visibility)
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
        # Cannot add Qt-style resources when not using Qt
        if(Qt == nil)
            raise "Cannot add Qt-style resources from #{root} when not using Qt"
        end
        # If we don't have a QRC source task yet, create one
        qrcXml = @build.artifact("qrc_#{@name}.qrc")
        qrcSourcePath = @build.artifact("qrc_#{@name}.cpp")
        if(@qrcResources == nil)
            @qrcResources = []
            # Generate a QRC resource script (as XML)
            file qrcXml => [@build.componentDir] do |t|
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
            file qrcSourcePath => [qrcXml] do |t|
                puts "rcc #{@name}"
                Tc.rcc(qrcXml, t.name, @name)
            end
            # Include it in the build
            qrcObj = File.basename(qrcSourcePath) + Tc.objectExt(qrcSourcePath)
            compileSource(qrcSourcePath, qrcObj)
        end

        resourcePatterns = Util.joinPaths([[root], patterns])
        resources = Rake::FileList[resourcePatterns]
            .exclude(*Util.joinPaths([[root], ResourceExcludePatterns]))
            .exclude(*Util.joinPaths([[root], excludePatterns]))
            .select {|f| !File.directory?(f)}
        @qrcResources << {root: root, files: resources}
        # qrcXml depends on all of these files
        task qrcXml => resources

        self
    end

    # Include a specific source file in the object.
    #
    # Most of the time, use .source(path) to specify a whole directory instead;
    # specific files are used for deps like Breakpad when the whole directory is
    # not needed.
    def sourceFile(sourcePath)
        objPath = sourcePath + Tc.objectExt(sourcePath)
        compileSource(sourcePath, objPath)
    end

    # Include a specific header file in the object.  Header files obviously
    # aren't compiled themselves, this just checks for a possible moc task.
    #
    # Most of the time, use .source(path) to specify a whole directory instead.
    def headerFile(headerPath)
        if needsMoc?(headerPath)
            mocSrcPath = headerPath.ext(".moc.cpp")
            @builders.each do |b|
                archMocSrcPath = archArtifact(mocSrcPath, b)
                archMocObjPath = archMocSrcPath + Tc.objectExt(mocSrcPath)
                b.mocHeader(headerPath, archMocSrcPath, archMocObjPath)
            end
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
    # - Platform-specific subdirectories (win, mac, linux, posix), are included
    #   automatically
    # - moc rules are generated for headers in this directory, the resulting
    #   files are compiled and linked into the module
    #
    # Occasionally, a module may export its source directory as an include
    # directory.  This usually happens for third-party modules, where we
    # expect to be able to `#include <7z.h>`, etc. in source.  Pass :export
    # for includeVisibility to export an include path.
    def source(path, includeVisibility = nil)
        # If this path is an exported include directory, it's also an internal
        # include directory, because `#include <...>` needs to work on those
        # files within this project too.
        #
        # Otherwise, it is neither.  Source files can always `#include "..."` to
        # get files from their current directory, and if the component includes
        # multiple source directories, we don't want them implicitly including
        # each other's files (instead consider `#include "../file.h"` or
        # `#include <module/src/file.h>`.
        include(path, :export) if includeVisibility == :export

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
