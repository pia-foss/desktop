require_relative '../executable.rb'
require_relative '../model/probe.rb'
require_relative '../macinfoplist.rb'
require_relative '../archive.rb'
require_relative '../product/version.rb'
require_relative '../model/build.rb'
require_relative '../util/dsl.rb'

module PiaMacOS
    extend BuildDSL

    def self.defineTargets(version, stage)
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

        # Install the kext
        FileList['deps/split_tunnel/mac/PiaKext.kext/**/*'].each do |f|
            target = f.gsub('deps/split_tunnel/mac/', 'Contents/Resources/')
            stage.install(f, target)
        end

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
            sh "cd \"#{File.dirname(brandedInstallSh)}\" && xxd -i \"#{File.basename(brandedInstallSh)}\" - >>\"#{File.absolute_path(brandedInstallShDef)}\""
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
            .use(version.export)
            .define('BUILD_COMMON')
            .source('common/src')
            .source('common/src/builtin')
            .sourceFile('extras/openvpn/mac_openvpn_helper.cpp')
            .useQt('Network')
            .install(stage, :bin)

        # Generated files for the app bundle
        bundleGenerated = Build.new('bundle-generated')
        infoPlist = bundleGenerated.artifact('Info.plist')

        infoPlistContent = MacInfoPlist.defaultPlist(version.productName,
                                                     version.productName,
                                                     Build::ProductIdentifier)
        infoPlistContent['CFBundleIconFile'] = 'app.icns'
        infoPlistContent['CFBundleURLTypes'] = [
            {
                'CFBundleTypeRole': 'Viewer',
                'CFBundleURLName': "#{Build::ProductIdentifier}.uri",
                'CFBundleURLSchemes': ["#{Build::Brand}vpn"],
                'CFBundleURLIconFile': 'app.icns'
            }
        ]
        infoPlistContent['LSUIElement'] = true
        infoPlistContent['NSSupportsAutomaticGraphicsSwitching'] = true
        infoPlistContent['SMPrivilegedExecutables'] = {
            "#{Build::ProductIdentifier}.installhelper":
                "identifier #{Build::ProductIdentifier}.installhelper " +
                "and certificate leaf[subject.CN] = \"#{ENV['PIA_CODESIGN_CERT']}\" " +
                "and info [#{Build::ProductIdentifier}.version] = \"#{version.version}\""
        }
        infoPlistContent["#{Build::ProductIdentifier}.version"] = version.version

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
            File.write(infoPlist, MacInfoPlist.renderPlistXml(infoPlistContent))
        end
        stage.install(infoPlist, 'Contents/')

        # Generate a PkgInfo, it just contains 'APPL????'
        pkgInfo = bundleGenerated.artifact('PkgInfo')
        file pkgInfo => bundleGenerated.componentDir do |t|
            File.write(pkgInfo, infoPlistContent['CFBundlePackageType'] +
                                infoPlistContent['CFBundleSignature'])
        end
        stage.install(pkgInfo, 'Contents/')

        # Create the support tool bundle, linking to the pia-support-tool binary
        # in the main app bundle
        stBundleGenerated = Build.new('support-tool-bundle-generated')
        stInfoPlist = stBundleGenerated.artifact('Info.plist')

        stInfoPlistContent = MacInfoPlist.defaultPlist("#{version.productShortName} Support Tool",
                                                       "#{Build::Brand}-support-tool",
                                                       "#{Build::ProductIdentifier}.support-tool")
        stInfoPlistContent['CFBundleIconFile'] = 'app.icns'
        stInfoPlistContent['LSUIElement'] = false
        stInfoPlistContent['NSSupportsAutomaticGraphicsSwitching'] = true
        file stInfoPlist => [stBundleGenerated.componentDir,
                             version.artifact('version.txt'),
                             version.artifact('brand.txt')] do |t|
            File.write(stInfoPlist, MacInfoPlist.renderPlistXml(stInfoPlistContent))
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

        sh *macdeployArgs
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
                sh *codesignArgs, '--sign', cert, '--verbose', '--force', installHelper

                sh *codesignArgs, '--deep', '--sign', cert, '--verbose', '--force', bundle
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
                sh './scripts/notarize-macos.sh', notarizeZip,
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
