require 'net/http'
require 'openssl'
require 'open3'
require_relative 'rake/toolchain/compiler_database'

# Don't mess with bundler on CI servers - we currently only need
# bundler when developing locally to install extra gems for
# testing (rspec) and debugging (pry)
if ENV['GITHUB_CI'].nil?
    begin
        print "Installing gems (if needed)..."
        # Run bundler to install the gems in our Gemfile
        # We use this style of execution (which captures stdout/stderr rather than "sh" to prevent noise)
        # This style returns output as a string (which we just discard)
        Open3.capture3("bundle install")
        puts "done!"
    rescue
        puts "Bundler not installed - install bundler with: sudo gem install bundler -v 2.3.17"
        exit 1
    end

    require 'rubygems'
    require 'bundler/setup'

    # Auto-require all the gems in our gemfile
    Bundler.require(:default)
end

# Bring in our build system classes
require_relative 'rake/buildsystem'

# Fail if LFS wasn't set up.  Otherwise, builds can actually succeed but fail
# with confusing problems at runtime.
#
# To do this, check a large file that's stored with LFS and is always used.  If
# the size is too small, it's the LFS pointer file instead of the actual file.
if(File.size('client/res/img/map.png') < 1024)
    raise "Repository was not cloned with LFS.  Set up Git LFS and update; see README.md"
end

version = PiaVersion.new

# Artifacts - These are the final outputs preserved from CI builds.
# _Everything_ we keep from CI builds is included here.  (This does not include
# dev tools, etc., as those are only used in dev and can be built locally.  It
# does include non-shipping artifacts like library SDK packages,
# debugging symbols, translation exports, etc.)
artifacts = Install.new('artifacts')

# 'tools' builds utility applications that are just part of the development
# workflow.  These are staged to tools/ in the output directory.  These are not
# part of the build process itself or any shipped artifacts.
#
# Note that these are still built for the target platform - some of them are
# library test harnesses used on the target, etc.
toolsStage = Install.new("tools")
desc "Build tools (test harnesses, dev tools, etc.)"
task :tools => toolsStage.target do |t|
    puts "built tools"
end

# Export the source directory as an include directory to permit
# `#include <version.h>`, `#include <brand.h>`.
versionlib = Executable.new("#{Build::Brand}-version", :static)
    .source(version.artifact(''), :export)

# Dependency components
deps = {
    jsonmcpp: nil,
    embeddablewg: nil
}

# "JSON for Modern C++" (henceforth "jsonmcpp"), is a header-only library, just
# define a component.
#
# Apple Clang claims C++17 support, but std::filesystem wasn't actually provided
# until macOS 10.15/iOS 13.0 (we target 10.14/12.0).  Tell the lib to use C++14
# only, which just drops the std::filesystem::path and std::string_view
# conversions
deps[:jsonmcpp] = Component.new(nil)
    .include('deps/jsonmcpp/include')
    .define('JSON_HAS_CPP_11')
    .define('JSON_HAS_CPP_14') # but not 17 due to the above

# embeddable-wg-library is a small WireGuard client library for Linux.  The
# implementation can set up a WireGuard connection using the Linux kernel's
# native support.
#
# The header is used on all platforms as a common descriptor of a WireGuard
# connection.  The implementation is only used on Linux (it's in src/linux)
deps[:embeddablewg] = Executable.new("embeddable-wg-library", :static)
    .source('deps/embeddable-wg-library/src', :export)

# Shared libraries - included in both the PIA artifacts and the library dev
# artifacts
kappsModules = {
    core: nil,
    net: nil,
    regions: nil
}

# We usually use hyphens (pia-clientlib, pia-daemon, etc.), but the KApps libs
# use an underscore because the framework names on XNU targets really need to be
# valid C identifiers.
kappsModules[:core] = Executable.new("kapps_core", :dynamic)
    .define('BUILD_KAPPS_CORE')
    .source('kapps_core/src')
    .include('.', :export)
    .include('kapps_core/api', :export)
    .use(versionlib.export)
    .use(deps[:jsonmcpp], :export)

if Build.macos?
    kappsModules[:core]
        .framework('Foundation')
# Linux needs explicit linking of pthreads library for std::thread support
# (Not needed on Android, bionic includes pthreads)
elsif Build.linux?
    kappsModules[:core]
        .lib('pthread')
end

kappsModules[:net] = Executable.new("kapps_net", :dynamic)
    .define('BUILD_KAPPS_NET')
    .source('kapps_net/src')
    .include('kapps_net/api', :export)
    .use(versionlib.export)
    .use(kappsModules[:core].export, :export)

kappsModules[:regions] = Executable.new("kapps_regions", :dynamic)
    .define('BUILD_KAPPS_REGIONS')
    .source('kapps_regions/src')
    .include('kapps_regions/api', :export)
    .use(versionlib.export)
    .use(kappsModules[:core].export, :export)

# Library SDK artifact for reusable modules
kappsLibs = Install.new('kapps-libs')
# Include the version file
kappsLibs.install(version.artifact('version.txt'), '')
kappsLibs.install('KAPPS-LIBS.md', '')
# On Windows, the DLLs/PDBs go into bin/, while import libraries go in lib/.
# On macOS, Linux, Android, and iOS, so/dylib files go in lib/.
libBinDir = Build.selectPlatform('bin/', 'lib/', 'lib/', 'lib/', 'lib/')
# On Android, we have to ship the NDK's C++ shared library ourselves
kappsLibs.install(Executable::Tc.findLibrary(Build::TargetArchitecture, 'libc++_shared.so'), libBinDir) if Build.android?
kappsModules.each do |name, executable|
    # Copy the DLL/so/dylib (and symlinks on macOS/Linux)
    executable.install(kappsLibs, libBinDir)

    # On Windows, we also need the PDB and import library; these are side
    # effects of the build/link tasks.  We don't need any other artifacts on
    # macOS/Linux.
    if Build.windows? then
        # Bit kludgy, but just have Rake assume that the PDB/LIB depend on the
        # DLL output so these install tasks are correctly ordered after the
        # DLL build.  The DLL build will modify these too if it's rebuilt, so
        # we don't need to do any touch/etc. to propagate modified times.
        targetLib = executable.target.ext('.lib')
        targetPdb = executable.target.ext('.pdb')
        task targetLib => executable.target
        task targetPdb => executable.target
        kappsLibs.install(targetLib, 'lib/')
        kappsLibs.install(targetPdb, libBinDir)
    end

    # Include all API headers (but not internal headers)
    # Eventually all modules will be in kapps_#{name}, but a few haven't yet
    # been renamed from dtop-#{name}
    FileList["kapps_#{name}/api/kapps_#{name}/*", "dtop-#{name}/api/kapps_#{name}/*"].each do |h|
        kappsLibs.install(h, "inc/kapps_#{name}/")
    end
end

desc "Build and stage libraries"
task :libs_stage => kappsLibs.target do |t|
    puts "staged kapps libraries"
end

# Build a zipped archive of our libs folder
libsArchive = Build.new('kapps-libs-artifact')
libsArchivePkg = libsArchive.artifact("kapps-libs-#{version.packageSuffix}.zip")
file libsArchivePkg => [libsArchive.componentDir, :libs_stage] do |t|
    Archive.zipDirectory(kappsLibs.dir, libsArchivePkg)
end

desc "Build library SDK package"
task :libs_archive => libsArchivePkg do |t|
    puts "built kapps library development package"
end

# These test applications test the libraries - devs can use these to test the
# library internals; C-linkage APIs, etc.  All libraries and test apps are
# staged in tools/
kappsModules.each do |name, executable|
    executable.install(toolsStage, :lib)
end
toolsStage.install(Executable::Tc.findLibrary(Build::TargetArchitecture, 'libc++_shared.so'), libBinDir) if Build.android?

Executable.new('kapps-lib-test')
    .source('tools/kapps-lib-test')
    .use(kappsModules[:net].export)
    .install(toolsStage, :bin)

Executable.new('kapps-specs')
    .source('tools/kapps-specs')
    .use(kappsModules[:net].export)
    .install(toolsStage, :bin)

Executable.new('kapps-regions-test')
    .source('tools/kapps-regions-test')
    .use(kappsModules[:net].export)
    .use(kappsModules[:regions].export)
    .install(toolsStage, :bin)

artifacts.install(libsArchivePkg, '')
artifacts.install(version.artifact('version.txt'), '')

desc "Build all artifacts - includes unit tests only when coverage artifacts are possible"
task :artifacts => artifacts.target do |t|
    puts "produced artifacts:"
    FileList[File.join(artifacts.dir, '**/*')].each do |f|
        puts " - #{File.basename(f)} - #{File.size(f)} bytes"
    end
end

# For XNU targets, export each kapps library as a framework with a module
# definition for use from Swift in Xcode
if Build.xnuKernel?
    PiaFrameworks.defineTargets(version, kappsModules, artifacts)
end

# Add universal targets to :all
desc "Build everything, including tasks with no artifacts (tools, tests, etc.)"
task :all => [:tools, :artifacts] do |t|
    puts "build finished"
end

desc "Clean the output directory (for this configuration)"
task :clean do |t|
    puts "cleaning #{Build::BuildDir}"
    FileUtils.rm_rf(Build::BuildDir)
end

# In rare cases, we just want to run the probes without actually building
# anything - some scripts do this to find Qt to extract Qt debug symbols, etc.
# Since probes are always run when the rakefile is invoked, the :probe task just
# does nothing.
desc "Run probes only (find toolchains and dependencies, etc.)"
task :probe

# Load family-specific targets
if Build.desktop?
    PiaDesktop.defineTargets(version, versionlib, deps, kappsModules, artifacts, toolsStage)
else
    # There are no platform-specific targets for other platforms right now.  Just
    # hook up the default task, which is platform-dependent.
    task :default => :libs_stage
end

desc "Run specs for the Rake build system"
task :build_specs do
    Util.shellRun "bundle exec rspec rake/spec/"
end

desc "Create compilation database compile_commands.json"
task :compile_commands => :stage do
    CompilerDatabase.build_compile_commands
end
task :default => :compile_commands

desc "Run headless integration tests"
task :headless do
  chdir('headless_tests') {Util.shellRun("rspec .")}
end