class DumpSyms

    def self.dump_syms(binary_path, syms_dir, replicate_with_suffix=[])
        FileUtils.mkdir_p(syms_dir)
        tmp_symbol_file = File.join(syms_dir, "#{File.basename(binary_path)}.tmp")
        exit_status = run_dump_syms(binary_path, tmp_symbol_file)
        if (exit_status != 0)
            raise "Failed to dump symbols from #{binary_path}"
        end
        # Read the first line of the output file, contains module information
        module_hash, binary_name = File.open(tmp_symbol_file) {|f| f.readline}.split[3,4]
        clean_binary_name = File.basename(binary_name, ".pdb")
        symbol_target_path = File.join(syms_dir, binary_name, module_hash)
        FileUtils.mkdir_p(symbol_target_path)
        FileUtils.cp(tmp_symbol_file, File.join(symbol_target_path, clean_binary_name + ".sym"))
        
        replicate_with_suffix.each do |suffix|
            # Add a versioned copy for better crash debug support
            versioned_outpath = File.join(syms_dir, binary_name + suffix, module_hash)
            FileUtils.mkdir_p(versioned_outpath)
            versioned_file_path = File.join(versioned_outpath, clean_binary_name + suffix + ".sym")
            FileUtils.cp(tmp_symbol_file, versioned_file_path)
        end
        FileUtils.rm(tmp_symbol_file)
    end

    private
    def self.run_dump_syms(binary_path, symbol_path)
        args = []
        env_vars = {}
        if Build.windows?
            # Add msdia140.dll to PATH so it doesn't have to be registered.
            path = [
                File.join(Executable::Tc.toolchainPath.gsub('/', '\\'), 'DIA SDK\bin'),
                ENV['PATH']
            ]
            
            run_dump_syms_with_fallback(binary_path, symbol_path, [], {"ENV"=>path.join(';')})
        elsif Build::TargetArchitecture == :universal
            exit_status = 0
            Build::PlatformUniversalArchitectures[Build::Platform].each do |arch|
                # x86_64 has no infix for compatibility with macOS x86_64
                # builds, other arches get an infix
                arch_prefix = (arch == :x86_64) ? "" : "#{arch}."
                arch_symbol_path = File.join(File.dirname(symbol_path), arch_prefix + File.basename(symbol_path))
                # dump_syms dumps only one arch at a time for universal binaries,
                # specify the arch with -a
                exit_status += run_dump_syms_with_fallback(binary_path, arch_symbol_path, ["-a", arch.to_s])
            end
            exit_status
        else
            run_dump_syms_with_fallback(binary_path, symbol_path)
        end
    end

    # Runs dump_syms from deps. If that fails it will try to fallback to any dump_syms
    # installed in the system. This is for compatibility with newer environments used 
    # during development.
    def self.run_dump_syms_with_fallback(binary_path, symbol_path, args=[], env_vars={})
        dump_syms_cmd = File.absolute_path("deps/dump_syms/dump_syms#{Build::selectDesktop('.exe', '_mac', '_linux.bin')}")
        system(env_vars, dump_syms_cmd, *args, binary_path, out: symbol_path, err: File::NULL)
        exit_status = $?.exitstatus
        if (exit_status == 0 && File.size(symbol_path) > 0)
            return 0
        end
        # If the dump_syms command failed it could still be that we are in a dev
        # environment and there could be a version of dump_syms that works in PATH.
        backup_dump_syms_cmd = "dump_syms"
        system(env_vars, backup_dump_syms_cmd, "-h", out: File::NULL, err: File::NULL)
        backup_exit_status = $?.exitstatus
        if (backup_exit_status <= 1)
            # There is a version in path, we can run it.
            system(env_vars, backup_dump_syms_cmd, *args, binary_path, out: symbol_path, err: File::NULL)
            return $?.exitstatus
        end
        exit_status
    end
end