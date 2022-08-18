require_relative 'model/build.rb'
require_relative 'util/dsl.rb'

# Stage an installation in a build directory.  Artifacts can be added with
# install().
class Install
    include BuildDSL

    # Known platform-specific directories that can be passed to install() as a
    # targetPath
    # TODO - Check mobile
    PlatformDirs = {
        bin: Build.selectPlatform('/', 'Contents/MacOS/', 'bin/', 'bin/', 'Contents/iOS/'),
        lib: Build.selectPlatform('/', 'Contents/Frameworks/', 'lib/', 'lib/', 'Contents/iOS/'),
        res: Build.selectPlatform('/', 'Contents/Resources/', 'share/', 'share/', 'Contents/Resources/')
    }

    # Create Install with the name of the staging directory.
    def initialize(name)
        @name = name
        @build = Build.new(name)
        @targetTask = "install-#{@name}"

        # Create a final task to clean up any extra files that might have been
        # left from a prior build.  Tasks that work on the installation
        # directory after it is staged can depend on this one.
        task @targetTask => @build.componentDir do |t|
            puts "install: #{@name}"
            extras = FileList[@build.artifact('**/*')]
            extras.exclude(*t.prerequisites)
            # Delete in reverse order so we delete files before their containing
            # directories (FileList results are already sorted)
            extras.reverse_each do |e|
                File.directory?(e) ? Dir.delete(e) : File.delete(e)
            end
        end
    end

    # Install an artifact.
    #
    # For example:
    #  install(target, :bin)  # Install to platform's executable directory
    #  install(target, :bin, "alt-name") # Install to platform's executable directory with different name
    #
    #  # Install to specific directory (relative to stage root), reusing file name
    #  install(target, '/specific/dir/')
    #
    #  # Install to specific directory (relative to stage root) with specific name
    #  install(target, '/specific/dir/alt-name')
    #  install(target, '/specific/dir', 'alt-name') # equivalent
    #  install(target, '/specific/dir/', 'alt-name') # equivalent
    #
    # - sourcePath is a path to an artifact
    # - targetPath can be:
    #   - a symbol for a platform directory - :bin, :lib, or :res
    #   - a path relative to the install root (paths starting with '/' are still
    #     treated as relative to the install root, '/' means to install to the
    #     root itself)
    #   If targetPath ends in a '/' (or is a platform directory), and targetName
    #   is not given, the file name from sourcePath is used.  Otherwise, the
    #   last component of targetPath determines the name.
    #   If targetPath is '' exactly, the file is installed to the install root
    #   with its existing name (as if targetPath was '/')
    # - targetName can be specified to rename the file (useful when targetPath
    #   is :bin/:lib/:res).
    #
    # Avoid using './', '../', or backslashes in targetPath, this doesn't
    # currently normalize the path (this may fool the parent directory tasks)
    def install(sourcePath, targetPath, targetName = nil)
        known = PlatformDirs[targetPath]
        targetPath = known if known != nil

        # Treat a target path of '' like '/' - install to the root with the
        # existing name.  (The leading slash is otherwise optional, so this
        # makes it optional when installing to the root too.)
        if(targetPath == '')
            targetPath = '/'
        end

        if(targetName != nil)
            targetPath = File.join(targetPath, targetName)
        end

        if(targetPath.end_with?('/'))
            targetPath = File.join(targetPath, File.basename(sourcePath))
        end

        # Create the target directory if the artifact is in a subdirectory.
        # Don't do this if the artifact is in the install root, because we'd
        # create a circular dependency of the install root on itself in that
        # case.
        artifactPath = @build.artifact(targetPath)
        artifactDir = File.dirname(artifactPath)

        # Copy the artifact
        file artifactPath => sourcePath do |t|
            puts "copy: #{artifactPath}"
            FileUtils.rm_f(artifactPath)
            # Use copy_entry to preserve symlinks (libcrypto.so/libssl.so)
            FileUtils.copy_entry(sourcePath, artifactPath)
        end

        # The install target depends on the artifact
        task @targetTask => artifactPath

        # Create tasks for each parent directory for the staged artifact, so
        # the install task keeps these directories.
        prevTarget = artifactPath
        nextSubdir = File.dirname(targetPath)
        # We get '.' at the end if the path didn't start with a slash, or '/' if
        # it did.  (install() treats both as relative to the staging directory)
        while(nextSubdir != '.' && nextSubdir != '/')
            stagedSubdir = @build.artifact(nextSubdir)
            directory stagedSubdir
            # The install target depends on all subdirectories
            task @targetTask => stagedSubdir
            # The prior task (artifact or descendant directory) depends on this
            # one
            task prevTarget => stagedSubdir

            prevTarget = stagedSubdir
            nextSubdir = File.dirname(nextSubdir)
        end

        task prevTarget => @build.componentDir

        self
    end

    def target
        @targetTask
    end

    def dir
        @build.componentDir
    end

    def artifact(name)
        @build.artifact(name)
    end
end
