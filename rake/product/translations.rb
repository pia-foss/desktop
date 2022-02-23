require_relative '../executable.rb'
require_relative '../model/build.rb'

# Define translation targets:
# - the client's translation resource file
# - the exported en_US translation file for Translation Platforms
def defineTranslationTargets(stage, artifacts)
    tsBuild = Build.new('translations')

    # There are no translatable strings in daemon/.  There's one in
    # common/src/builtin/error.cpp - this should be cleaned up, it's a trace
    # string and shouldn't be translated.  There are no others in common/.
    inputFolders = ['client', 'extras/installer/win', 'common/src/builtin']
    # Build a list of translatable files, just to indicate the dependencies of
    # this task to Rake.  (lupdate just takes entire directories and walks the
    # directories itself.)
    #
    # Virtually all of our translatable strings appear in .qml files (client)
    # or .rc files (Windows installer).
    #
    # .mm files can't contain translations - lupdate can't handle these on
    # platforms other than macOS, and the translation artifacts need to be the
    # same on all platforms.
    #
    # .inl files could theoretically contain translations if they only apply to
    # the installer, but this is unlikely to be useful.
    inputExtensions = ['js', 'qml', 'rc']
    # There are a few translations in .cpp/.h files, but this is rare, and it's
    # even rarer that they change.  Don't mark these prerequisites in Rake to
    # reduce rebuilds in the common case that only cpp/h files are changed, but
    # do include them for lupdate.
    almostUnusedExtensions = ['cpp', 'h']
    inputPatterns = Util.joinPaths([inputFolders, ['**'], inputExtensions.map {|e| "*.#{e}"}])
    translatableFiles = FileList[*inputPatterns]

    # List the translation files that we need to update
    translations = FileList['client/ts/*.ts']
    # Only include pseudotranslations in debug builds
    translations.exclude('client/ts/ro.ts', 'client/ts/ps.ts') unless Build::debug?

    translations.each do |ts|
        file tsBuild.artifact(File.basename(ts)) => [ts, tsBuild.componentDir] do |t|
            puts "import: #{ts}"
            importedContent = File.read(ts, encoding: "UTF-8")
            File.write(t.name, importedContent)
        end
    end
    importedTs = translations.map { |t| tsBuild.artifact(File.basename(t)) }

    # Most of the work goes into one large task targeting translations.qrc.
    # lupdate updates the ts artifacts in-place, and then we export .qm files
    # with lrelease.
    #
    # (Rake's FileTask can't model a task with more than one output, so we don't
    # tell it about the .qm files and let it check the mtime of the .qrc file
    # instead.)
    #
    # lrelease has to be run on each file though.
    qrcFile = tsBuild.artifact('translations.qrc')
    file qrcFile => (importedTs + translatableFiles) do |qrcTask|
        # Use uiTr instead of qsTr - see ClientQmlContext::retranslate().
        # We _only_ accept uiTr() and ignore qsTr().  This ensures that using qsTr()
        # by mistake results in a string that does not translate at all (instead of
        # one that translates at startup but does not retranslate, which is much
        # harder to catch in testing).
        args = [
            Executable::Qt.tool('lupdate'),
            '-locations', 'absolute', '-extensions',
            (inputExtensions + almostUnusedExtensions).join(','),
            '-tr-function-alias', 'qsTr=uiTr,qsTranslate=uiTranslate',
            '-disable-heuristic', 'sametext', '-disable-heuristic', 'similartext',
            '-disable-heuristic', 'number'
        ]
        args += inputFolders
        args << '-ts'
        args += importedTs
        # lupdate updates all ts files in one pass, so we only need to run it once.
        sh *args

        lreleaseFixed = [
            Executable::Qt.tool('lrelease'), '-silent', '-removeidentical',
            '-compress'
        ]
        importedTs.each do |t|
            # lupdate has a bug and doesn't support absolute paths, but regardless
            # we want the actual paths to be relative to the client/ts directory.
            tsContent = File.read(t)
            tsContent.gsub!('"../../../../../../../client', '"..')
            File.write(t, tsContent)
            # Run lrelease on each .ts file to generate .qm files
            puts "lrelease: #{t}"
            sh *(lreleaseFixed + [t, '-qm', t.ext('.qm')])
        end

        # Generate the qrc file.  We do want to rewrite this even if it hasn't
        # changed, because this indicates that the .qm files have changed.
        qrcContent = '<RCC><qresource prefix="/translations">'
        qrcContent << "\n"
        qrcContent << importedTs.map do |t|
            qm = File.basename(t.ext('.qm'))
            "<file alias=\"client.#{qm}\">#{qm}</file>\n"
        end.join('')
        qrcContent << '</qresource></RCC>'
        File.write(qrcTask.name, qrcContent)
    end

    # Compile the qrc file into an rcc binary resource file
    rccFile = tsBuild.artifact('translations.rcc')
    file rccFile => qrcFile do |t|
        puts "rcc: #{qrcFile}"
        sh Executable::Qt.tool('rcc'), '-binary', qrcFile, '-o', t.name
    end

    stage.install(rccFile, :res)

    exportBuild = Build.new('translations_export')
    sourceFile = exportBuild.artifact('pia_desktop.ts')
    updatedEnUs = tsBuild.artifact('en_US.ts')

    file sourceFile => [updatedEnUs, exportBuild.componentDir] do |t|
        puts "export: #{t.name}"
        FileUtils.copy_file(updatedEnUs, t.name)
    end

    artifacts.install(sourceFile, '')

    # This is an export artifact - it doesn't go in the staging directory and
    # isn't an installer, it's just exported from CI so we can ship it off for
    # translations
    desc "Build translation export artifact (for submission to translators)"
    task :export => [sourceFile] do |t|
        puts "exported translation artifacts"
    end
end
