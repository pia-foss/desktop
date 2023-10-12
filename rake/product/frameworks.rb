module PiaFrameworks
    extend BuildDSL

    def self.moduleFrameworkName(modName)
        # The framework names are the same as the library names.  This is
        # important due to a number of interconnected restrictions Xcode/LLVM
        # place on framework bundles and modules:
        # * LLVM understands '#include <{framework}/...>' and locates the header
        #   module inside that framework
        #   -> Since the API headers are also built without modules on other
        #      platforms, the framework name must be the same as the header
        #      directory (api/kapps_core, etc.)
        # * The mobule's binary name must be the same as the framework name
        #   -> In theory it seems possible to specify an arbitrary name in the
        #      Info.plist, but it's pretty difficult to actually get right.
        # * The module name and framework name must be a valid C identifier
        #   -> This necessiates "kapps_core" instead of our preferred
        #      "kapps-core", etc.
        #   -> Supposedly the Swift bridging mechanism permits non-C-identifier
        #      names by replacing the invalid characters with '_', but the LLVM
        #      module file syntax does not permit such a name at all.
        #
        # So the framework names are "kapps_core", etc.  This is inconsistent
        # with the title-case style that's conventional for Darwin, but that
        # doesn't seem to cause any problems, and it is _much_ easier if this
        # name is the same on all platforms.
        "kapps_#{modName}"
    end

    def self.defineTargets(version, kappsModules, artifacts)
        # Build the arguments to install_name_tool to update all references to
        # any other library
        installNameUpdate = ['install_name_tool']
        installNameUpdate += kappsModules.flat_map do |name, executable|
            modFwName = moduleFrameworkName(name)
            ['-change', "@executable_path/../Frameworks/#{File.basename(executable.target)}",
             "@rpath/#{modFwName}.framework/#{modFwName}"]
        end

        frameworks = Install.new('kapps-frameworks')
        frameworks.install(version.artifact('version.txt'), '')
        frameworks.install('KAPPS-LIBS.md', '')

        kappsModules.each do |name, executable|
            fwName = moduleFrameworkName(name)

            # Generate files needed for this framework
            fwGenerated = Build.new("frameworks-gen/#{fwName}")
            # Generate Info.plist
            infoPlist = fwGenerated.artifact('Info.plist')
            file infoPlist => [fwGenerated.componentDir] do |t|
                content = MacInfoPlist.new(fwName, fwName,
                                           "com.kape.client-frameworks.#{fwName}")
                    .framework.targetPlatform
                File.write(infoPlist, content.renderPlistXml)
            end
            # Generate a module map
            moduleMap = fwGenerated.artifact('module.modulemap')
            file moduleMap => [fwGenerated.componentDir] do |t|
                content = "framework module #{fwName} [extern_c] {\n" +
                    "  umbrella \".\"\n" +
                    "  module * {\n" +
                    "    export *\n" +
                    "  }\n" +
                    "}\n"
                File.write(moduleMap, content)
            end
            # Generate a copy of the dylib with the install name and references
            # to other dylibs updated
            dylib = fwGenerated.artifact(fwName)
            file dylib => [fwGenerated.componentDir, executable.target] do |t|
                FileUtils.cp(executable.target, dylib)
                # Update this module's install name and references to any other
                # modules
                Util.shellRun *installNameUpdate, '-id', "@rpath/#{fwName}.framework/#{fwName}", dylib
                Util.shellRun 'install_name_tool', '-rpath', '@loader_path/../Frameworks', '@loader_path/..', dylib
                # Delete Qt's rpath if it exists.  Only some modules have this
                # path, so it's fine if this fails.
                if Executable::Qt != nil
                    Util.shellRun "install_name_tool -delete_rpath '#{File.join(Executable::Qt.targetQtRoot, 'lib')}' '#{dylib}' || true"
                end
            end

            # Build a framework containing this module
            frameworks.install(infoPlist, "#{fwName}.framework/")
            frameworks.install(moduleMap, "#{fwName}.framework/Modules/")
            frameworks.install(dylib, "#{fwName}.framework/")
            # Install all of the C API headers.  The headers from
            # 'api/kapps_{name}' go directly into 'Headers' - then LLVM will
            # correctly map '#include <kapps_{name}/{header}.h>' to the
            # 'Headers/{header}.h' file in the 'kapps_{name}' framework.
            FileList["dtop-#{name}/api/kapps_#{name}/*", "kapps_#{name}/api/kapps_#{name}/*"].each do |h|
                frameworks.install(h, "#{fwName}.framework/Headers/")
            end

            # Copy the dSYM, this is generated as a side effect of the module's
            # task
            symbolFile = executable.target + ".dSYM/Contents/Resources/DWARF/" + File.basename(executable.target)
            symbolPlist = executable.target + ".dSYM/Contents/Info.plist"
            file symbolFile => executable.target
            file symbolPlist => executable.target
            frameworks.install(symbolFile, "#{fwName}.framework.dSYM/Contents/Resources/DWARF/#{fwName}")
            frameworks.install(symbolPlist, "#{fwName}.framework.dSYM/Contents/Info.plist")
        end

        desc "Build Darwin frameworks (staged, suitable for direct use)"
        task :frameworks => frameworks.target

        frameworksArchive = Build.new('frameworks-archive')
        frameworksArchivePkg = frameworksArchive.artifact("kapps-frameworks-#{version.packageSuffix}.zip")
        file frameworksArchivePkg => [:frameworks, frameworksArchive.componentDir] do |t|
            Archive.zipDirectory(frameworks.dir, frameworksArchivePkg)
        end
        artifacts.install(frameworksArchivePkg, '/')
        desc "Build Darwin frameworks artifact package"
        task :frameworks_archive => frameworksArchivePkg
    end
end
