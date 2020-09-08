require_relative 'component.rb'
require_relative 'build.rb'

# Generate a group of text files using a probe - information that doesn't come
# from clear file dependencies.
#
# For example:
#  - Git information is obtained from git
#  - The semantic version is hard-coded in the project
#
# The file contents are always computed, but the resulting files are only
# rewritten if they have actually changed.  The files are generated when
# evaluating the Rakefiles, not with actual tasks, since the dependencies can't
# be expressed to Rake.
#
# Other components can then use these files by using the files themselves as
# prerequisites (either implicitly via makedep or explicitly).  This will cause
# those components to be rebuilt if the generated files change.
class Probe
    def initialize(groupName)
        # Use a Build object to determine the build directory for this
        # component.
        # We don't use the actual directory task to create that directory
        # though, because Probe does not use tasks to generate these files.
        @build = Build.new("probe-#{groupName}")
        @groupName = groupName
        FileUtils.mkpath(@build.componentDir)
    end

    # Generate a file in the group build directory.  The content is always
    # computed (by the caller), but the file is only rewritten if it has
    # actually changed.
    def file(name, content)
        path = artifact(name)
        needed = true
        begin
            existingContent = File.read(path)
            needed = existingContent != content
        rescue
            # Just assume the file doesn't exist and needs to be written
        end
        if(needed)
            puts "generate #{name}"
            File.write(path, content)
        end
        self
    end

    # Get the path to a file that was generated
    def artifact(name)
        @build.artifact(name)
    end

    # Export a component definition to use the generated files as C++ headers.
    def export()
        Component.new(nil).include(@build.componentDir)
    end
end
