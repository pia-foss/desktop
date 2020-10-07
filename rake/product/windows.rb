require_relative '../executable.rb'
require_relative '../archive.rb'
require_relative '../product/version.rb'
require_relative '../model/build.rb'
require_relative '../util/dsl.rb'

module PiaWindows
    extend BuildDSL

    SignCertFile = ENV['PIA_SIGNTOOL_CERTFILE']
    SignPassword = ENV['PIA_SIGNTOOL_PASSWORD']
    SignThumbprint = ENV['PIA_SIGNTOOL_THUMBPRINT']
    CanSign = Build::release? && (SignCertFile != nil || SignThumbprint != nil)

    # Base setup for uninstaller/installer; these are built from the same source
    # with either INSTALLER or UNINSTALLER defined
    def self.winstaller(name, version)
        target = Executable.new(name, :executable)
            .gui
            .define('_STATIC_CPPLIB')
            .runtime(:static)
            .use(version.export)
            .source('extras/installer/win')
            .source('extras/installer/win/tasks')
            .source('extras/installer/win/translations') # translated string resources
            .sourceFile("brands/#{Build::Brand}/brand_installer.rc")
            .linkArgs([
                "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'",
                # Specify these as delay-loaded since they're not "known DLLs" (i.e.
                # can be subject to executable path lookup rules)
                'delayimp.lib',
                '/DELAYLOAD:newdev.dll',
                '/DELAYLOAD:userenv.dll'
            ])
        # Include pseudolocalizations only in debug builds
        target.source('extras/installer/win/translations/debug') if Build::debug?
        target
    end

    # Find the MSVC/UCRT runtime files and add them to an installation target
    def self.installRuntime(target)
        arch = (Build::Architecture == :x86_64) ? 'x64' : Build::Architecture.to_s

        msvcLibs = [ 'msvcp140', 'msvcp140_1', 'vcruntime140' ]
        # vcruntime140_1.dll is required on x86_64 (SEH fix in VC runtime), but
        # does not exist at all on x86
        if(Build::Architecture == :x86_64)
            msvcLibs << 'vcruntime140_1'
        end

        crtDir = File.absolute_path(ENV['VCToolsRedistDir'])
        if(Build::debug?)
            crtDir = File.join(crtDir, 'debug_nonredist')
        end
        crtDir = File.join(crtDir, arch,
            Build::debug? ? 'Microsoft.VC142.DebugCRT' : 'Microsoft.VC142.CRT')

        msvcLibs.each do |l|
            libPath = File.join(crtDir, "#{l}#{Build.debug? ? 'd' : ''}.dll")
            target.install(libPath, '/')
        end

        ucrtPattern = ''
        if(Build::debug?)
            # Find the last SDK version
            # Normalize to / for FileList to work
            winSdkPath = File.absolute_path(ENV['WindowsSdkBinPath'])
            lastSdk = FileList[File.join(winSdkPath, '10.*')].max
            ucrtPattern = File.join(lastSdk, arch, 'ucrt/*.dll')
        else
            winSdk = File.absolute_path(ENV['WindowsSdkDir'])
            ucrtPattern = File.join(winSdk, 'Redist/ucrt/DLLs', arch, '*.dll')
        end

        FileList[ucrtPattern].each { |l| target.install(l, '/') }
    end

    # Invoke windeployqt on Windows
    def self.winDeploy(qmlDirs, binaryFilePaths)
        args = [Executable::Qt.tool('windeployqt'), '-verbose', '0']
        args += qmlDirs.flat_map{|d| ['--qmldir', File.absolute_path(d)]}
        args += [
            '--no-webkit2', '--no-angle', '--no-compiler-runtime',
            '--no-translations', '--no-opengl-sw'
        ]
        args += binaryFilePaths
        sh *args
    end

    # Invoke signtool on Windows - signs one time
    # - files - absolute paths to files to sign, with Windows separators
    # - first - whether this is the first signature or an additional signature
    # - hash - hash algorithm to use in signature, 'sha1' or 'sha256'
    # - useTimestamp - whether to use a timestamping authority
    # - description - if non-nil, file description passed to signtool
    def self.signtool(files, first, hash, useTimestamp, description)
        args = [
            'signtool', # Placed in PATH by vcvars
            'sign'
        ]
        args << '/as' unless first # append signature if not the first one
        args << '/fd'
        args << hash
        if(useTimestamp)
            args << '/tr'
            args << 'http://timestamp.digicert.com'
            args << '/td'
            args << hash
        end
        # Cert args - can be specified with a file + password or a thumbprint,
        # comes from environment variables
        if(SignCertFile != nil)
            args << '/f'
            args << SignCertFile
            if(SignPassword != nil)
                args << '/p'
                args << SignPassword
            end
        else
            args << '/sha1'
            args << SignThumbprint
        end
        # File description
        if(description != nil)
            args << '/d'
            args << description
        end

        sh *(args + files)
    end

    # Double-sign files using both SHA-1 and SHA-256.
    # There's a slight error here that we're still using a SHA-256 certificate
    # in the SHA-1 signature - there's no way to specify two separate certs.
    # However, only Windows 7 RTM lacks SHA-256 signature support (and it's
    # available in an update), so this isn't going to be fixed at this point.
    def self.doubleSign(files, useTimestamp, description)
        # Get absolute, Windows-style paths
        files = files.map {|f| File.absolute_path(f).gsub!('/', '\\')}

        signtool(files, true, 'sha1', useTimestamp, description)
        signtool(files, false, 'sha256', useTimestamp, description)
    end

    # Define additional installable artifacts and targets for Windows.
    def self.defineTargets(version, stage)
        # This module is used by the client to indirectly access Windows Runtime
        # APIs.  The client remains compatible with Windows 7 by only loading this
        # module on 8+.  The Windows Runtime APIs themselves are spread among
        # various modules, so this level of indirection avoids introducing a hard
        # dependency on any of those modules from the client itself.
        winrtsupport = Executable.new("#{Build::Brand}-winrtsupport", :dynamic)
            .include('common/src/builtin') # For common.h
            .include('common/src/win') # For win_com.h / win_util.h
            .headerFile('common/src/win/win_com.h')
            .sourceFile('common/src/win/win_com.cpp')
            .headerFile('common/src/win/win_util.h')
            .sourceFile('common/src/win/win_util.cpp')
            .source('extras/winrtsupport/src')
            .useQt(nil) # Core only
            .install(stage, '/')

        # Service stub used to replace the Dnscache service for split tunnel
        # DNS; see win_dnscachecontrol.cpp
        winsvcstub = Executable.new("#{Build::Brand}-winsvcstub")
            .include('common/src/builtin')
            .include('common/src/win')
            .headerFile('common/src/win/win_util.h')
            .sourceFile('common/src/win/win_util.cpp')
            .source('extras/winsvcstub/src')
            .useQt(nil)
            .install(stage, '/')

        # MSVC and UCRT runtime file - enumerate the files and add install targets
        installRuntime(stage)

        # Drivers
        FileList["deps/tap/win/#{Build::Architecture}/win*/*"].each do |f|
            # Install to win7/* or win10/*
            winVerDir = File.basename(File.dirname(f))
            stage.install(f, "tap/#{winVerDir}/")
        end
        FileList["brands/#{Build::Brand}/wintun/#{Build::Architecture}/*.msi"].each do |f|
            stage.install(f, 'wintun/')
        end
        FileList["deps/wfp_callout/win/#{Build::Architecture}/win*/*"].each do |f|
            winVerDir = File.basename(File.dirname(f))
            stage.install(f, "wfp_callout/#{winVerDir}/")
        end

        # OpenVPN updown script (used for 'static' configuration method)
        stage.install('extras/openvpn/win/openvpn_updown.bat', '/')

        # zip.exe, used by support tool on Windows
        stage.install('deps/zip/zip.exe', '/')

        # Windows uninstaller
        uninstall = winstaller('uninstall', version)
            .define('UNINSTALLER')
            .install(stage, '/')
    end

    def self.defineIntegtestArtifact(version, integtestStage, artifacts)
        # MSVC and UCRT runtime files
        installRuntime(integtestStage)

        # Deploy Qt for the integration tests
        task :integtest_deploy => integtestStage.target do |t|
            winDeploy([], [File.join(integtestStage.dir, "#{Build::Brand}-integtest.exe")])
        end

        # Create a ZIP containing the deployed integ tests as the final artifact
        integtestBuild = Build.new('integtest')
        integtest = integtestBuild.artifact("#{version.integtestPackageName}.zip")
        file integtest => [:integtest_deploy, integtestBuild.componentDir] do |t|
            Archive.zipDirectory(integtestStage.dir, integtest)
        end

        # Preserve the integtest distribution artifact
        artifacts.install(integtest, '')

        task :integtest => integtest
    end

    # Define the task to build the Windows installer artifact.  This depends on the
    # staged output and becomes a dependency of the :installer task.
    def self.defineInstaller(version, stage, artifacts)
        # This task (and the following tasks to compress / sign / link installer)
        # will always run since the staging task always runs.
        task :windeploy => stage.target do |t|
            # The CLI executable is excluded from the deploy binaries, for some
            # reason including that prevents windeployqt from deploying any QtQuick
            # dependencies.  Fortunately, it doesn't have any specific dependencies
            # of its own.
            deployExes = ["#{Build::Brand}-client.exe", "#{Build::Brand}-service.exe"]
            winDeploy(['client/res/components', 'extras/support-tool/components'],
                      deployExes.map {|f| File.join(stage.dir, f)})
        end

        task :winsign => :windeploy do |t|
            # Nothing to do if we can't sign
            if(CanSign)
                fileDescriptions = {}
                fileDescriptions["#{Build::Brand}-client.exe"] = version.productName
                fileDescriptions["#{Build::Brand}-service.exe"] = "#{version.productName} Service"
                fileDescriptions["uninstall.exe"] = "#{version.productName} Uninstaller"

                namedFiles = []
                unnamedExes = []
                unnamedDlls = []

                FileList[File.join(stage.dir, '*')].each do |f|
                    if(fileDescriptions.include?(File.basename(f)))
                        namedFiles << f
                    elsif(File.extname(f) == '.exe')
                        unnamedExes << f
                    elsif(File.extname(f) == '.dll')
                        unnamedDlls << f
                    end
                end

                namedFiles.each do |f|
                    doubleSign([f], true, fileDescriptions[File.basename(f)])
                end
                doubleSign(unnamedExes, true, nil) unless unnamedExes.empty?
                doubleSign(unnamedDlls, false, nil) unless unnamedDlls.empty?
            end
        end

        # Build the payload with 7-zip.
        payloadBuild = Build.new('payload')
        payload = payloadBuild.artifact('payload.7z')
        file payload => [:winsign, payloadBuild.componentDir] do |t|
            Archive.zipDirectoryContents(stage.dir, payload)
        end

        # Create a resource script to include the payload data in the installer.
        # This depends on the payload so the installer has an indirect
        # dependency.
        payloadRc = payloadBuild.artifact('payload.rc')
        file payloadRc => [payload, payloadBuild.componentDir] do |t|
            File.write(t.name, "1337 RCDATA \"#{File.absolute_path(payload)}\"\n")
        end

        # Build the installer using the payload
        install = winstaller(version.packageName, version)
            .define('INSTALLER')
            .source('deps/lzma/src')
            .sourceFile(payloadRc)

        # Sign the installer and put it in a predicatable location for job
        # artifacts to pick up
        installerBuild = Build.new('installer')
        signedInstaller = installerBuild.artifact(File.basename(install.target))
        file signedInstaller => [install.target, installerBuild.componentDir] do |t|
            puts "sign: #{signedInstaller}"
            FileUtils.copy(install.target, signedInstaller)
            if(CanSign)
                doubleSign([signedInstaller], true, "#{version.productName} Installer")
            end
        end

        artifacts.install(signedInstaller, '')

        task :installer => signedInstaller
    end

    def self.collectSymbols(version, stage, debugSymbols)
        symbolsDir = debugSymbols.artifact("symbols-#{version.packageName}")
        FileUtils.mkdir(symbolsDir)
        FileList.new(File.join(stage.dir, '**/*.exe'), File.join(stage.dir, '**/*.dll'))
            .each do |f|
                FileUtils.copy_entry(f, File.join(symbolsDir, File.basename(f)))
                modName = File.basename(f, '.*')
                pdbPath = File.join(Build::BuildDir, modName, modName + '.pdb')
                if(File.exist?(pdbPath))
                    FileUtils.copy_entry(pdbPath, File.join(symbolsDir, modName + '.pdb'))
                end
            end
    end
end
