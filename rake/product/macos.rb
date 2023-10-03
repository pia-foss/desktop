require_relative '../executable.rb'
require_relative '../model/probe.rb'
require_relative '../macinfoplist.rb'
require_relative '../archive.rb'
require_relative '../product/version.rb'
require_relative '../model/build.rb'
require_relative '../util/dsl.rb'

module PiaMacOS
    extend BuildDSL

    def self.defineTargets(version, stage, kappsModules, commonlib, clientlib)
        # Define a probe to identify the codesign certificate in use, and other
        # variables that affect codesigning.
        # The file content doesn't really matter, but this is a good way to hook
        # this up to a file mtime, so rake will know to rebuild things that
        # depend on this when the cert specified has changed.  (This actually
        # can change frequently because it's easy to forget PIA_CODESIGN_CERT=
        # when building from CLI.)
        codesignProbe = Probe.new('codesign')
        codesignProbe.file('codesign.txt', "#{ENV['PIA_CODESIGN_CERT']}\n#{ENV['PIA_BRANCH_BUILD']}\n#{ENV['PIA_ALWAYS_NOTARIZE']}")
        codesignProbeArtifact = codesignProbe.artifact('codesign.txt')

        # Install the app icon
        stage.install("brands/#{Build::Brand}/icons/app.icns", :res)

        # Brand and install shell scripts
        shellProcessed = Build.new('shell-processed')
        FileList['extras/installer/mac/*.sh'].each do |f|
            basename = File.basename(f)
            # install.sh becomes vpn-installer.sh for historical reasons
            basename = 'vpn-installer.sh' if basename == 'install.sh'

            branded = shellProcessed.artifact(basename)
            file branded => [f, shellProcessed.componentDir] do |t|
                version.brandFile(f, t.name)
                FileUtils.chmod('a+x', t.name)
            end
            stage.install(branded, :res)
        end

        # Brand and install pf config files
        pfProcessed = Build.new('pf-processed')
        FileList['daemon/res/pf/*'].each do |f|
            branded = pfProcessed.artifact("#{Build::ProductIdentifier}.#{File.basename(f)}")
            file branded => [f, pfProcessed.componentDir] do |t|
                version.brandFile(f, t.name)
            end
            stage.install(branded, 'Contents/Resources/pf/')
        end

        # Generated files for install helper
        installGenerated = Build.new('install-generated')

        # Create a copy of the install script embedded in a header file, for use
        # in the install helper (so it doesn't have to shell out to a
        # possibly-modified script on uninstall)
        brandedInstallSh = shellProcessed.artifact('vpn-installer.sh')
        brandedInstallShDef = installGenerated.artifact('vpn-installer-sh.cpp')
        file brandedInstallShDef => [brandedInstallSh, installGenerated.componentDir] do |t|
            # Run xxd in the input directory, so the variable name doesn't
            # include that path
            File.write(brandedInstallShDef,
                        "extern unsigned char vpn_installer_sh[];\n" +
                        "extern unsigned int vpn_installer_sh_len;\n")
           Util.shellRun "cd \"#{File.dirname(brandedInstallSh)}\" && xxd -i \"#{File.basename(brandedInstallSh)}\" - >>\"#{File.absolute_path(brandedInstallShDef)}\""
        end

        # Helper installed with SMJobBless to install/repair/update the
        # application
        installHelper = Executable.new(Build::ProductIdentifier + '.installhelper')
            .noDefaultInfoPlist # Custom plists below
            .use(version.export)
            .sourceFile(brandedInstallShDef)
            .source('extras/installer/mac/helper')
            .framework('Foundation')
            .framework('Security')
            .install(stage, 'Contents/Library/LaunchServices/')

        # Embed an Info.plist and Launchd.plist into the helper binary.  These
        # need to be preprocessed for branding.
        #
        # Most guides suggest linking these in with the -sectcreate option to
        # Clang, but this instead writes out an assembler file and assembles it
        # to create the proper text sections.
        FileList['extras/installer/mac/helper/*.plist.template'].each do |f|
            basename = File.basename(f, '.plist.template')
            brandedPlist = installGenerated.artifact(basename + '.plist')
            file brandedPlist => [f, version.artifact('version.txt'),
                                  version.artifact('brand.txt'),
                                  codesignProbeArtifact,
                                  installGenerated.componentDir] do |t|
                version.brandFile(f, t.name)
            end

            # The assembly file has a dependency on the branded plist so it'll
            # be reassembled if the plist changes.
            plistAsm = installGenerated.artifact(basename + '.plist.s')
            file plistAsm => [brandedPlist, installGenerated.componentDir] do |t|
                sectName = "__#{basename}_plist"
                asm = ".section __TEXT,#{sectName}\n" +
                    ".incbin \"#{File.absolute_path(brandedPlist)}\"\n"
                File.write(plistAsm, asm)
            end

            # Compile that section into the helper
            installHelper.sourceFile(plistAsm)
        end

        unquarantine = Executable.new("#{Build::Brand}-unquarantine")
            .sourceFile('extras/installer/mac/unquarantine.cpp')
            .install(stage, :bin)

        openvpnHelper = Executable.new("#{Build::Brand}-openvpn-helper")
            .use(clientlib.export)
            .sourceFile('extras/openvpn/mac_openvpn_helper.cpp')
            .useQt('Network')
            .install(stage, :bin)

        # Generated files for the app bundle
        bundleGenerated = Build.new('bundle-generated')
        infoPlist = bundleGenerated.artifact('Info.plist')

        infoPlistContent = MacInfoPlist.new(version.productName,
                                            version.productName,
                                            Build::ProductIdentifier)
            .macosApp
            .set('CFBundleIconFile', 'app.icns')
            .set('CFBundleURLTypes', [
                    {
                        'CFBundleTypeRole': 'Viewer',
                        'CFBundleURLName': "#{Build::ProductIdentifier}.uri",
                        'CFBundleURLSchemes': ["#{Build::Brand}vpn"],
                        'CFBundleURLIconFile': 'app.icns'
                    }
                ])
            .set('LSUIElement', true)
            .set('NSSupportsAutomaticGraphicsSwitching', true)
            .set('SMPrivilegedExecutables', {
                    "#{Build::ProductIdentifier}.installhelper":
                        "identifier #{Build::ProductIdentifier}.installhelper " +
                        "and certificate leaf[subject.CN] = \"#{ENV['PIA_CODESIGN_CERT']}\" " +
                        "and info [#{Build::ProductIdentifier}.version] = \"#{version.version}\""
                })
            .set("#{Build::ProductIdentifier}.version", version.version)

        # The Info.plist has dependencies on version.txt and brand.txt so it
        # will be recreated if the version/brand info changes.  The dependency
        # on the codesign probe ensures that we update it if PIA_CODESIGN_CERT
        # changes.
        # Theoretically we could actually read the values from those files, but
        # there's no sense overcomplicating this.
        file infoPlist => [bundleGenerated.componentDir,
                           version.artifact('version.txt'),
                           version.artifact('brand.txt'),
                           codesignProbeArtifact] do |t|
            File.write(infoPlist, infoPlistContent.renderPlistXml)
        end
        stage.install(infoPlist, 'Contents/')

        # Generate a PkgInfo, it just contains 'APPL????'
        pkgInfo = bundleGenerated.artifact('PkgInfo')
        file pkgInfo => bundleGenerated.componentDir do |t|
            File.write(pkgInfo, infoPlistContent.get('CFBundlePackageType') +
                                infoPlistContent.get('CFBundleSignature'))
        end
        stage.install(pkgInfo, 'Contents/')

        # Create the support tool bundle, linking to the pia-support-tool binary
        # in the main app bundle
        stBundleGenerated = Build.new('support-tool-bundle-generated')
        stInfoPlist = stBundleGenerated.artifact('Info.plist')

        stInfoPlistContent = MacInfoPlist.new("#{version.productShortName} Support Tool",
                                              "#{Build::Brand}-support-tool",
                                              "#{Build::ProductIdentifier}.support-tool")
            .macosApp
            .set('CFBundleIconFile', 'app.icns')
            .set('LSUIElement', false)
            .set('NSSupportsAutomaticGraphicsSwitching', true)
        file stInfoPlist => [stBundleGenerated.componentDir,
                             version.artifact('version.txt'),
                             version.artifact('brand.txt')] do |t|
            File.write(stInfoPlist, stInfoPlistContent.renderPlistXml)
        end
        stBundleContents = "Contents/Resources/#{Build::Brand}-support-tool.app/Contents/"
        stage.install(stInfoPlist, stBundleContents)
        # Install app.icns and qt.conf in the nested bundle
        stage.install("brands/#{Build::Brand}/icons/app.icns", stBundleContents + 'Resources/')
        stage.install('extras/support-tool/mac-bundle/qt.conf', stBundleContents + 'Resources/')
        # Symlink in the support tool executable
        stSymlink = bundleGenerated.artifact("#{Build::Brand}-support-tool")
        file stSymlink => bundleGenerated.componentDir do |t|
            FileUtils.ln_sf("../../../../MacOS/#{Build::Brand}-support-tool", t.name)
        end
        stage.install(stSymlink, File.join(stBundleContents, 'MacOS/'))

        # TODO - Integtest Info.plist and PkgInfo
    end

    # Run two processes with arguments - redirect both stdout and stderr from
    # the first into the stdin of the second.
    #
    # (Avoids having to use the "shell" form of system() when argument quoting
    # is nontrivial)
    def self.systemPipeline(firstArgs, secondArgs)
        firstPid = nil
        secondPid = nil
        IO.pipe do |readPipe, writePipe|
            firstPid = spawn(*firstArgs, {out: writePipe.fileno, err: writePipe.fileno})
            secondPid = spawn(*secondArgs, in: readPipe.fileno)
        end
        statuses = []
        Process.wait(firstPid)
        statuses.push([firstArgs, $?])
        Process.wait(secondPid)
        statuses.push([secondArgs, $?])

        statuses.each do |s|
            if ! s[1].success?
                raise "Invocation of #{s[0].join(' ')} failed: #{s[1]}"
            end
        end
    end

    def self.macdeployEnv
        # macdeployqt very nearly works as-is on a universal build of Qt.
        # install_name_tool correctly applies the desired change to every arch,
        # and otool -L dumps dependencies for all arches, etc., so it does
        # deploy correctly.
        #
        # It has slight issues when parsing otool -L when it reaches the second
        # arch header, which it assumes are more dependency lines.
        #
        # The architecture header is correctly ignored because it doesn't
        # parse as a dependency line.  It produces a noisy error, but
        # macdeployqt does the right thing.
        #
        # The second install name line though _does_ match the format of a
        # dependency line.  This too nearly works, treating every library as
        # a dependency of itself (which is correctly ignored), _except_ for
        # some plugins whose install names are unadorned file names, like
        # libqmlfolderlistmodelplugin.dylib and several others (inspect them
        # with otool -L and compare with the results for the QtCore framework
        # module, etc.
        #
        # In that specific case, macdeployqt thinks the library should be found
        # in the library search path and tries to find
        # /usr/lib/libqmlfolderlistmodelplugin.dylib, which (hopefully) does
        # not exist.
        #
        # Even then, it still ignores the file when it can't be found and still
        # produces a working deployment.  However, for robustness, make
        # macdeployqt use a shim around otool that only inspects one
        # architecture.  The changes are made with install_name_tool, which
        # still correctly applies to all architectures.
        #
        # This assumes that the dependencies are the same for all architectures.
        # We need to do this whenever the Qt build used is universal, even if
        # we're not building a universal target.
        return {} if Executable::Qt.targetQtArch != :universal
        analyzeArch = (Build::TargetArchitecture == :universal) ?
            Build::PlatformUniversalArchitectures[:macos][0] :
            Build::TargetArchitecture
        return {
            "PATH" => "./tools/otool-single-arch:#{ENV["PATH"]}",
            "OTOOL_SINGLE_ARCH" => analyzeArch.to_s
        }
    end

    def self.macdeploy(bundleDir, qmlDirs)
        # Find all the binaries that need to be passed to macdeployqt.
        # - Ignore libssl, libcrypto, etc. - only include binaries starting
        #   with the brand code
        # - Ignore the bundle executable, the default executable is implied
        #   to macdeployqt (matters for pia-integtest, and if a brand name would
        #   happen to begin with the brand code)
        bundleName = File.basename(bundleDir)
        deployBins = FileList[File.join(bundleDir, "Contents/MacOS/#{Build::Brand}*")]
            .reject { |f| File.symlink?(f) || File.basename(f) == bundleName }

        macdeployArgs = [Executable::Qt.tool('macdeployqt'), bundleDir]
        macdeployArgs += deployBins.map { |f| "-executable=#{f}" }
        macdeployArgs += qmlDirs.map { |d| "-qmldir=#{d}" }
        macdeployArgs << '-no-strip'

        # FileUtils::sh doesn't accept environment variables
        puts macdeployArgs.join(" ")
        system(macdeployEnv, *macdeployArgs)
        if !$?.success?
            raise "macdeployqt failed with result #{$?.to_s}"
        end

        # Qt 5.15.2 doesn't actually support cross-arch builds, and macdeployqt
        # has some issues with them.  It's close enough that we can fix it up,
        # but we do not ship these builds.  The build we ship uses universal
        # macdeployqt to deploy universal Qt libs, which is close enough to a
        # native build that it works.
        if(Executable::Qt.targetQtRoot != Executable::Qt.hostQtRoot)
            puts "WARNING: Cross-architecture (non-universal) builds are not supported on macOS"
            # The only issue seems to be that macdeployqt deploys the wrong
            # plugins - it deploys host arch plugins, not target arch.  Replace
            # them with the correct plugins.
            FileList[File.join(bundleDir, 'Contents/PlugIns/*')].each do |groupDir|
                FileList[File.join(groupDir, '*.dylib')].each do |pluginFile|
                    # Get the last two components - 'group/plugin.dylib'
                    group = File.basename(groupDir)
                    plugin = File.basename(pluginFile)
                    pluginRelPath = File.join(group, plugin)
                    deployedPlugin = File.join(bundleDir, 'Contents/PlugIns', pluginRelPath)

                    # The correct plugin is usually in <Qt>/plugins/<groups>/<plugin>.dylib.
                    # QML plugins though can be in any number of components under <Qt>/qml.
                    # The names of QML plugins must be unique enough to find the
                    # file - after all, macdeployqt dumps them all in the same
                    # directory.
                    if(group == "quick")
                        # This will also match a .dSYM somewhere, but the actual
                        # dylib always sorts first
                        correctPlugin = FileList[File.join(Executable::Qt.targetQtRoot, 'qml/**', plugin)].first
                    else
                        correctPlugin = File.join(Executable::Qt.targetQtRoot, 'plugins', pluginRelPath)
                    end

                    # Replace the deployed (wrong-arch) plugin with the correct one
                    puts "fix deployed plugin: #{pluginRelPath} -> #{correctPlugin}"
                    FileUtils.cp(correctPlugin, deployedPlugin)
                    # Note that macdeployqt also changes load entries with
                    # install_name_tool to use @rpath instead of @loader_path.
                    # This is _not_ done here; the @loader_path relative path is
                    # also correct in PIA's app bundle.
                end
            end
        end
    end

    def self.defineIntegtestArtifact(version, integtestStage, artifacts)
        integtestBuild = Build.new('integtest')
        bundle = integtestBuild.artifact("#{Build::Brand}-integtest.app")

        task :integtest_deploy => [integtestStage.target, integtestBuild.componentDir] do |t|
            FileUtils.rm_rf(bundle) if File.exist?(bundle)
            FileUtils.cp_r(integtestStage.dir, bundle)

            macdeploy(bundle, [])
        end

        # Create the integtest ZIP artifact
        integtest = integtestBuild.artifact("#{version.integtestPackageName}.zip")
        file integtest => [:integtest_deploy, integtestBuild.componentDir] do |t|
            Archive.zipDirectory(bundle, integtest)
        end

        artifacts.install(integtest, '')

        task :integtest => integtest
    end

    def self.defineInstaller(version, stage, artifacts)
        # Skip notarization for feature branches (PIA_BRANCH_BUILD defined to
        # something other than 'master'), but PIA_ALWAYS_NOTARIZE can force
        # notarization.
        notarizeBuild = !ENV['PIA_BRANCH_BUILD'] || ENV['PIA_BRANCH_BUILD'] == '' ||
                        ENV['PIA_BRANCH_BUILD'] == 'master' ||
                        ENV['PIA_ALWAYS_NOTARIZE'] == '1'
        # Can't notarize if credentials are not set
        if(!ENV['PIA_APPLE_ID_EMAIL'] || !ENV['PIA_APPLE_ID_PASSWORD'])
            notarizeBuild = false
        end

        installerBuild = Build.new('installer')

        bundle = installerBuild.artifact(File.basename(stage.dir))

        # Copy the staged installation.  macdeployqt can't be re-run a second
        # time, we have to delete and re-copy this to deploy.
        task :macdeploy => [stage.target, installerBuild.componentDir] do |t|
            puts "deploy: #{File.basename(bundle)}"

            FileUtils.rm_rf(bundle) if File.exist?(bundle)
            FileUtils.mkdir_p(bundle)

            # The final /. in the source path copies the contents of the bundle,
            # since we already created the app directory
            FileUtils.cp_r(File.join(stage.dir, '.'), bundle)

            macdeploy(bundle, ['client/res/components', 'extras/support-tool/components'])
            Dir.glob(File.join(bundle, "Contents/**/*.dSYM/**/*.dylib")).each { |f| File.delete(f) }
        end

        task :macsign => :macdeploy do |t|
            cert = ENV['PIA_CODESIGN_CERT']
            if(!cert)
                puts "Not signing, build will have to be manually installed"
                puts "Set PIA_CODESIGN_CERT to enable code signing, and see README.md for more information"
            else
                codesignArgs = ['codesign']
                # If the build will be notarized, sign with the hardened runtime
                # (required for notarization).  Don't do this when not notarizing
                # so a dev build can still be made with a self-signed cert (hardened
                # runtime requires a team ID)
                if(notarizeBuild)
                    codesignArgs << '--options=runtime'
                end

                # codesign --deep does not find the install helper, probably because
                # it's in Library/LaunchServices
                installHelper = File.join(bundle, 'Contents/Library/LaunchServices',
                                          "#{Build::ProductIdentifier}.installhelper")
                Util.shellRun *codesignArgs, '--sign', cert, '--verbose', '--force', installHelper

                Util.shellRun *codesignArgs, '--deep', '--sign', cert, '--verbose', '--force', bundle
            end
        end

        task :macnotarize => :macsign do |t|
            if(!notarizeBuild)
                puts "Skipping notarization for feature branch build."
            else
                # Zip the app with the original name temporarily to upload for
                # notarization
                notarizeZip = bundle.ext('.zip')
                Archive.zipDirectory(bundle, notarizeZip)

                # Perform notarization
               Util.shellRun './scripts/notarize-macos.sh', notarizeZip,
                    Build::ProductIdentifier, bundle

                # The original app bundle was stapled, we don't need the
                # temporary zip any more
                File.delete(notarizeZip)
            end
        end

        # Create the ZIP package artifact
        installerPackage = installerBuild.artifact("#{version.packageName}.zip")
        file installerPackage => [:macnotarize, installerBuild.componentDir] do |t|
            # Temporarily add " Installer" to the bundle directory
            installerBundle = installerBuild.artifact("#{version.productName} Installer.app")
            FileUtils.rm_rf(installerBundle)
            FileUtils.mv(bundle, installerBundle)
            # Clean all zips so they don't accumulate as commits are made
            FileUtils.rm(Dir.glob(installerBuild.artifact('*.zip')), force: true)
            Archive.zipDirectory(installerBundle, installerPackage)
            FileUtils.mv(installerBundle, bundle)
        end

        artifacts.install(installerPackage, '')

        task :installer => installerPackage
    end
end
