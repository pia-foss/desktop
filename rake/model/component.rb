# Define a component for use in an executable module.  Components can represent:
# - other compiled executable modules
# - modules found on the system (like Qt components)
# - other generated files to be used in a module (like generated header files)
#
# Executable.use() applies the macros, include directores, etc., from the
# Component to that Executable.
class Component
    # If the component is built by a task, specify the name of that task when
    # creating a Component.  This allows Executable.use() to create a
    # dependency on that task.
    #
    # If the component is not built by a task, pass nil so no dependency will be
    # created.
    def initialize(task)
        @task = task
        @macros = []
        @includeDirs = []
        @libPaths = []
        @libs = []
        # Further components used as dependencies
        @dependencies = []
        # Frameworks - Mac-specific, analogous to libs for this purpose
        @frameworkPaths = []
        @frameworks = []
    end

    # Define an exported macro.  Can be a macro name with no value, or
    # 'NAME=value', like Executable.define()
    def define(macro)
        @macros.push(macro)
        self
    end

    # Define an include directory.
    def include(dir)
        @includeDirs.push(File.absolute_path(dir))
        self
    end

    # Define an exported library path
    def libPath(dir)
        @libPaths.push(File.absolute_path(dir))
        self
    end

    # Specify the name of an exported library
    def lib(name)
        @libs.push(name)
        self
    end

    # Specify a dependent component.  Any module referencing this component also
    # references the dependencies automatically.  For example, this is used when
    # this component's headers include headers from another dependency
    # component, like kapps_net referencing kapps_core.
    def dependency(component)
        @dependencies.push(component)
        self
    end

    # Define an exported framework path (Mac-specific)
    def frameworkPath(dir)
        @frameworkPaths.push(File.absolute_path(dir))
        self
    end

    # Specify the name of an exported framework (Mac-specific)
    def framework(name)
        @frameworks.push(name)
        self
    end

    # Get the exported values - used by Executable.use()
    def task()
        @task
    end
    def macros()
        @macros
    end
    def includeDirs()
        @includeDirs
    end
    def libPaths()
        @libPaths
    end
    def libs()
        @libs
    end
    def dependencies()
        @dependencies
    end
    def frameworkPaths()
        @frameworkPaths
    end
    def frameworks()
        @frameworks
    end
end
