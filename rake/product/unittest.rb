require_relative '../executable.rb'
require_relative '../util/dsl.rb'
require 'json'

module PiaUnitTest
    extend BuildDSL

    Tests = [
        'apiclient',
        'check',
        'connectionconfig',
        'exec',
        'json',
        'jsonrefresher',
        'jsonrpc',
        'latencytracker',
        'linebuffer',
        'localsockets',
        'nearestlocations',
        'networkmonitor',
        'networktaskwithretry',
        'nodelist',
        'nullable_t',
        'originalnetworkscan',
        'openssl',
        'path',
        'portforwarder',
        'raii',
        'semversion',
        'settings',
        'subnetbypass',
        'tasks',
        'transportselector',
        'updatedownloader',
        'vpnmethod',
        'wireguarduapi'
    ].tap do |t|
        if Build.windows?
            t << 'wfp_filters'
        elsif Build.linux?
            t << 'splitdnsinfo'
        end
    end

    def self.defineTargets(version, artifacts)
        # The all-tests-lib library compiles all client and daemon code once to
        # be shared by all unit tests.
        # This duplicates the source directories and dependencies from the
        # various components.
        allTestsLib = Executable.new('all-tests-lib', :static)
            .define('BUILD_COMMON') # Common
            .source('common/src')
            .source('common/src/builtin')
            .useQt('Network')
            .define('BUILD_CLIENTLIB') # Clientlib
            .source('clientlib/src')
            .define('PIA_CLIENT', :export) # Client (resources not needed)
            .source('client/src')
            .source('client/src/nativeacc')
            .useQt('Qml')
            .useQt('Quick')
            .useQt('QuickControls2')
            .useQt('Gui')
            .useQt('Test')
            .define('PIA_DAEMON', :export) # Daemon
            .source('daemon/src')
            .source('deps/embeddable-wg-library/src')
            .resource('daemon/res', ['ca/*.crt'])
            .define('UNIT_TEST', :export) # Unit test
            .source('tests/src') # Unit test source
            .resource('tests/res', ['**/*']) # Unit test resources
            .use(version.export)
            .coverage(true) # Generate coverage information when possible
        if(Build.windows?)
            allTestsLib
                .useQt('xml')
                .useQt('WinExtras')
        elsif(Build.macos?)
            allTestsLib
                .include('extras/installer/mac/helper')
                .useQt('MacExtras')
        elsif(Build.linux?)
            allTestsLib
                .useQt('Widgets')
                .include('/usr/include/libnl3', :export)
        end

        # This task will depend on running all tests individually
        multitask :run_all_tests

        # Raw coverage data will be placed here if it can be generated
        coverageRawBuild = Build.new('coverage-raw')

        # Needed for coverage analysis below
        anyTestBin = nil

        Tests.each do |t|
            testExec = Executable.new("test-#{t}", :executable)
                .use(allTestsLib.export)
                .use(version.export)
                .useQt('Network') # Common
                .useQt('Qml') # Client
                .useQt('Quick')
                .useQt('QuickControls2')
                .useQt('Gui')
                .useQt('Test') # Test
                .define("TEST_MOC=\"tst_#{t}.moc\"")
                .sourceFile("tests/tst_#{t}.cpp")
                .include('.') # Some tests include headers using a path from the repo root due to historical QBS limitations
                .coverage(true)
            if(Build.windows?)
                testExec
                    .useQt('Xml')
                    .useQt('WinExtras')
                    .linkArgs(['/IGNORE:4099'])
            elsif(Build.macos?)
                testExec
                    .framework('AppKit')
                    .framework('CoreWLAN')
                    .framework('Security')
                    .framework('ServiceManagement')
                    .framework('SystemConfiguration') # Daemon dependencies
                    .useQt('MacExtras')
            elsif(Build.linux?)
                testExec.useQt('Widgets')
            end

            # Just grab the first test executable for this
            anyTestBin = testExec.target if anyTestBin == nil

            # Define an easily-spelled target to hook up individual tests to Qt
            # Creator
            task "test-#{t}" => [testExec.target]

            # Run the test.  If possible, coverage data is produced in the
            # coverage-raw directory.
            task "run-test-#{t}" => ["test-#{t}", coverageRawBuild.componentDir] do |task|
                puts "test: #{t}"
                testBin = testExec.target
                cmd = ''
                covData = coverageRawBuild.artifact("coverage_#{t}.raw")
                opensslLibPath = File.absolute_path(File.join('deps/built',
                                                              Build.selectPlatform('win', 'mac', 'linux'),
                                                              Build::TargetArchitecture.to_s))
                if Build.windows?
                    # Don't bother with covData, not supported on MSVC
                    path = [
                        File.join(Executable::Qt.targetQtRoot, 'bin'),
                        opensslLibPath,
                        ENV['PATH']
                    ]
                    cmd = Util.cmd("set \"PATH=#{path.join(';')}\" & set \"UNIT_TEST_LIB=#{opensslLibPath}\" & \"#{testBin}\"")
                elsif Build.macos?
                    cmd = "LLVM_PROFILE_FILE=\"#{covData}\" UNIT_TEST_LIB=\"#{opensslLibPath}\" \"#{testBin}\""
                elsif Build.linux?
                    libPath = [
                        File.join(Executable::Qt.targetQtRoot, 'lib'),
                        opensslLibPath
                    ]
                    cmd = "LD_LIBRARY_PATH=\"#{libPath.join(':')}\" LLVM_PROFILE_FILE=\"#{covData}\" UNIT_TEST_LIB=\"#{opensslLibPath}\" \"#{testBin}\""
                end
                sh cmd
            end

            task :build_all_tests => "test-#{t}"

            task :run_all_tests => "run-test-#{t}"
        end

        # Put an early dependency on all-tests-lib here - otherwise, the Rake
        # threads tend to get tied up on the parallel unit test jobs that all
        # depend on all-tests-lib, and only a few threads will actually be able
        # to do work.  We still want the run_all_tests task to build and run the
        # individual tests in parallel though.
        task :test => allTestsLib.target

        testTarget = nil
        if(!Executable::Tc.canExecute?)
            # Can't execute the unit tests - cross compiling with no emulation
            # support.  Just build the tests.
            testTarget = :build_all_tests
        elsif(Executable::Tc.coverageAvailable?)
            defineCoverageTargets(version, artifacts, coverageRawBuild, anyTestBin)
            # Hook up the 'test' target for use from the command line - include
            # coverage measurements in this target
            testTarget = :coverage
        else
            # Can execute the tests, but coverage isn't available, hook up the
            # :test target to :run_all_tests
            testTarget = :run_all_tests
        end

        task :test => testTarget do |t|
            puts "tests finished"
        end
    end

    def self.defineCoverageTargets(version, artifacts, coverageRawBuild, anyTestBin)
        # Merge and analyze coverage data if it was generated.  This depends on
        # all tests; it doesn't have specific file dependencies on the raw data
        # files because they can't be generated in all cases (requires clang>=6)
        coverageBuild = Build.new('coverage')
        merged = coverageBuild.artifact('unittest.profdata')
        listing = coverageBuild.artifact('unittest_listing.txt')
        report = coverageBuild.artifact('unittest_report.txt')
        coverage = coverageBuild.artifact('unittest_coverage.json')
        fileCoverage = coverageBuild.artifact('file_coverage.json')
        summaryJson = coverageBuild.artifact('pia_unittest_summary.json')
        # toolchainPath() is only provided by the clang toolchain; MSVC always
        # has coverageAvailable? == false so we won't do this for MSVC.
        llvmPath = Executable::Tc.toolchainPath

        file merged => [:run_all_tests, coverageBuild.componentDir] do |t|
            puts "merge test coverage data"
            sh File.join(llvmPath, 'llvm-profdata'), 'merge',
               *Tests.map{|test|coverageRawBuild.artifact("coverage_#{test}.raw")},
               '-o', merged
        end

        llvmCov = File.join(llvmPath, 'llvm-cov')

        file listing => [merged, coverageBuild.componentDir] do |t|
            # It seems like we'd want to pass all-tests-lib to llvm-cov here to
            # generate coverage for that code, but that doesn't work.  Instead,
            # pass any test binary.  This will include both all-tests-lib and
            # test-specific code; we'll exclude the test-specific code below.
            # LLVM will complain about conflicting data for each test's main(),
            # but that's OK.
            puts "generate coverage listing"
            sh "#{llvmCov} show \"#{anyTestBin}\" \"-instr-profile=#{merged}\" >\"#{listing}\""
        end

        file report => [merged, coverageBuild.componentDir] do |t|
            puts "generate coverage report"
            sh "#{llvmCov} report \"#{anyTestBin}\" \"-instr-profile=#{merged}\" >\"#{report}\""
        end

        file coverage => [merged, coverageBuild.componentDir] do |t|
            puts "generate coverage JSON export"
            sh "#{llvmCov} export -format=text \"#{anyTestBin}\" \"-instr-profile=#{merged}\" >\"#{coverage}\""
        end

        file fileCoverage => [coverage, coverageBuild.componentDir] do |t|
            # Read the exported JSON and generate the code coverage summary
            covData = JSON.parse(File.read(coverage))

            if(covData['data'].length != 1)
                raise "Expected one coverage object, found #{covData['data'].length}"
            end

            # The relevant source directories for code coverage
            srcDirs = ['client/src', 'common/src', 'daemon/src'].map{ |d| File.absolute_path(d) }

            json = covData['data'][0]['files']
                .select { |v| srcDirs.any? { |d| v["filename"].start_with?(d) } }
                .map { |v| [File.basename(v["filename"]), v['summary']['lines']['covered'],  v['summary']['lines']['count']] }
                .sort_by { |v| -v[1] }
                .each_with_object({}) { |v, o| o[v[0]] = [v[1], v[2]] }.to_json

            # Outputs a json file of the form: ["filename", lines_covered, line_count]
            # This aids us in debugging test covereage between platforms
            File.write(fileCoverage, json)
        end

        file summaryJson => [coverage, coverageBuild.componentDir] do |t|
            # Read the exported JSON and generate the code coverage summary
            covData = JSON.parse(File.read(coverage))

            if(covData['data'].length != 1)
                raise "Expected one coverage object, found #{covData['data'].length}"
            end

            # The relevant source directories for code coverage
            srcDirs = ['client/src', 'common/src', 'daemon/src'].map{|d|File.absolute_path(d)}

            platformSrcDirs = []
            platformCounts = {}
            [:win, :mac, :linux, :posix].each do |p|
                # Include a trailing '/' in the directory paths so sources like
                # '.../src/windowscaler.cpp' don't like like 'win' sources
                platformSrcDirs << {platform: p, dirs: Util.joinPaths([srcDirs, [p.to_s], ['']])}
                platformCounts[p] = {total: 0, covered: 0}
            end
            platformSrcDirs << {platform: :common, dirs: srcDirs.map{|d|File.join(d, '')}}
            platformCounts[:common] = {total: 0, covered: 0}

            # Go through the files in the report and add their line counts to the groups
            covData['data'][0]['files'].each do |f|
                platform = Util.find(platformSrcDirs) {|p| p[:dirs].any?{|d|f['filename'].start_with?(d)}}
                if(platform != nil)
                    counts = platformCounts[platform[:platform]]
                    counts[:total] += f['summary']['lines']['count']
                    counts[:covered] += f['summary']['lines']['covered']
                end
            end

            puts "Unit test code coverage:"
            platformCounts.each do |p, v|
                pct = 0
                cov = v[:covered].to_f
                tot = v[:total].to_f
                pct = cov / tot * 100.0 if tot > 0
                pctRound = pct.round(1)
                puts "--> #{p}: #{pctRound}% (#{cov}/#{tot})"
            end

            File.write(summaryJson, JSON.generate(platformCounts))
        end

        task :coverage => [merged, listing, report, coverage, fileCoverage, summaryJson]

        # The aim of this task is to locate and understand differences in test coverage between platforms
        # Run this on the file_coverage.json build artifacts from two different platforms
        desc "Perform a diff of two file_coverage.json files: rake coverage_diff file_coverage1.json file_coverage2.json"
        task :coverage_diff do
            # horrible hack to work around Rake argument limitations
            ARGV.each { |a| task a.to_sym do ; end }
            platformCov1 = JSON.load(File.read(ARGV[1]))
            platformCov2 = JSON.load(File.read(ARGV[2]))

            # Output is a json document with keys being the files with differing
            # coverage b/w the two platforms (if coverage is the same, we omit it from the output).
            # The values are tuples with the first element the difference in coverage
            # for that file, and the second element being the difference in total lines reported to be in that file.
            # The difference is calculated by arg1 - arg2
            Set.new(platformCov1.keys).intersection(platformCov2.keys)
                .each_with_object({}) { |v, o| o[v] = [platformCov1[v][0] - platformCov2[v][0], platformCov1[v][1] - platformCov2[v][1]] }
                .select { |k, v| v != [0, 0] }
                .tap { |v| pp v }
        end

        # Preserve all the coverage artifacts
        artifacts.install(merged, 'coverage/')
        artifacts.install(listing, 'coverage/')
        artifacts.install(report, 'coverage/')
        artifacts.install(coverage, 'coverage/')
        artifacts.install(summaryJson, 'coverage/')
        artifacts.install(fileCoverage, 'coverage/')
    end
end
