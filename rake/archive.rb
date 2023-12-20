require_relative 'model/build.rb'
require_relative 'util/dsl.rb'
require_relative 'util/util.rb'

module Archive
    extend BuildDSL

    Compression = Build.release? ? '9' : '3'
    SevenZip = Build.windows? ? File.join(ENV['PROGRAMFILES'], '7-Zip/7z.exe') : ''

    # Implementation of zipDirectory() and zipDirectoryContent().
    # Create a ZIP archive - basedir is the working directory (this part of the
    # path is not included in the archive).  'content' is written as-is into the
    # command line (not quoted) - the caller quotes it appropriately depending
    # on whether it's a literal filename or a glob.
    def self.zip(basedir, content, archive)
        workDir = File.absolute_path(basedir)
        archiveAbs = File.absolute_path(archive)

        File.delete(archive) if File.exist?(archive)
        if(Build::windows?)
            Util.shellRun Util.cmd("cd \"#{workDir}\" & \"#{SevenZip}\" a -mx#{Compression} \"#{archiveAbs}\" #{content}")
        else
            # '-y' preserves symlinks, important for Mac frameworks, Mac/Linux
            # versioned library symlinks, etc.
            Util.shellRun "cd \"#{workDir}\" && zip -q -y -#{Compression} -r \"#{archiveAbs}\" #{content}"
        end
    end

    # Create a ZIP archive containing the specified directory.  The directory
    # itself is included in the archive (so it's not a ZIP-bomb), but the path
    # leading to it is not.
    # On Mac/Linux, uses zip -<comp> -r.  On Windows, uses 7z.exe -mx<comp> a.
    def self.zipDirectory(directory, archive)
        zip(File.dirname(directory), "\"#{File.basename(directory)}\"", archive)
    end

    # Create an archive containing the contents of a directory - not the
    # directory itself.
    def self.zipDirectoryContents(directory, archive)
        # This is different from calling zipDirectory("#{directory}/*", archive),
        # because the * must not be quoted for the shell to expand it on
        # Mac/Linux.  (On Windows it doesn't matter whether it's quoted, 7z.exe
        # does the glob expansion either way.)
        zip(directory, '*', archive)
    end
end
