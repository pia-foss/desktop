require_relative '../model/build.rb'

module PiaBreakpad
    Macro = 'PIA_CRASH_REPORTING'
    Include = 'deps/breakpad'

    WindowsSources = [
        'deps/breakpad/client/windows/handler/exception_handler.cc',
        'deps/breakpad/client/windows/crash_generation/crash_generation_client.cc',
        'deps/breakpad/common/windows/guid_string.cc'
    ]

    MacSources = [
      'deps/breakpad/client/mac/handler/exception_handler.cc',
      'deps/breakpad/client/mac/crash_generation/crash_generation_client.cc',
      'deps/breakpad/client/mac/handler/minidump_generator.cc',
      'deps/breakpad/client/mac/handler/dynamic_images.cc',
      'deps/breakpad/client/mac/handler/breakpad_nlist_64.cc',
      'deps/breakpad/common/mac/string_utilities.cc',
      'deps/breakpad/common/mac/file_id.cc',
      'deps/breakpad/common/mac/macho_id.cc',
      'deps/breakpad/common/mac/macho_utilities.cc',
      'deps/breakpad/common/mac/macho_walker.cc',
      'deps/breakpad/common/mac/MachIPC.mm',
      'deps/breakpad/common/mac/bootstrap_compat.cc',
    ]

    LinuxSources = [
      'deps/breakpad/client/linux/log/log.cc',
      'deps/breakpad/client/linux/crash_generation/crash_generation_client.cc',
      'deps/breakpad/client/linux/dump_writer_common/thread_info.cc',
      'deps/breakpad/client/linux/dump_writer_common/ucontext_reader.cc',
      'deps/breakpad/client/linux/microdump_writer/microdump_writer.cc',
      'deps/breakpad/client/linux/minidump_writer/linux_dumper.cc',
      'deps/breakpad/client/linux/minidump_writer/linux_core_dumper.cc',
      'deps/breakpad/client/linux/minidump_writer/linux_ptrace_dumper.cc',
      'deps/breakpad/client/linux/minidump_writer/minidump_writer.cc',
      'deps/breakpad/client/linux/handler/minidump_descriptor.cc',
      'deps/breakpad/client/linux/handler/exception_handler.cc',
      'deps/breakpad/common/linux/guid_creator.cc',
      'deps/breakpad/common/linux/file_id.cc',
      'deps/breakpad/common/linux/elfutils.cc',
      'deps/breakpad/common/linux/elf_core_dump.cc',
      'deps/breakpad/common/linux/memory_mapped_file.cc',
      'deps/breakpad/common/linux/safe_readlink.cc',
      'deps/breakpad/common/linux/linux_libc_support.cc'
    ]

    PosixSources = [
      'deps/breakpad/client/minidump_file_writer.cc',
      'deps/breakpad/common/string_conversion.cc',
      'deps/breakpad/common/convert_UTF.c',
      'deps/breakpad/common/md5.cc',
    ]

    # Add Breakpad to an executable module.  This isn't exported as a Component
    # because the Breakpad sources are currently compiled into the module (maybe
    # they could be a static lib later), and this causes the module to export
    # PIA_CRASH_REPORTING (maybe exports from Components could be transitive)
    def self.add(executable)
        # Define PIA_CRASH_REPORTING and export it (so pia-clientlib exports
        # the definition to client)
        executable.define(Macro, true)
        executable.include(Include)
        executable.sourceFiles(WindowsSources) if Build.windows?
        executable.sourceFiles(MacSources) if Build.macos?
        executable.sourceFiles(LinuxSources) if Build.linux?
        executable.sourceFiles(PosixSources) if Build.posix?
    end
end
