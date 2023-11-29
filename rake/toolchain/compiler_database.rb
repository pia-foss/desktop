require 'json'
require_relative '../model/build'\

# Helper class for creating compilation databases. 
# These can be used by clang tools, checkers, IDEs, etc.
# For more information: https://clang.llvm.org/docs/JSONCompilationDatabase.html
class CompilerDatabase
    def self.create_fragment(source_file, output_file, params)
        # Ignore compiler launcher and compiler params
        params_start = Build::CompilerLauncher ? 2 : 1
        params = params[params_start..-1]
        # This follows the format of compile_commands files, but is only one entry
        fragment = {
            directory: Dir.getwd,
            file: source_file,
            output: output_file,
            arguments: params
        }
        fragment_filename = "#{output_file}.json_frag"
        File.open(fragment_filename, "w") {|f|
            f.write(fragment.to_json)
        }
    end

    # Builds a complete compile_commands.json file by combining all the fragments found in the build
    # directory. 
    def self.build_compile_commands
        File.open(File.join(Build::BuildDir, "compile_commands.json"), "w") { |out_file|
            out_file.write "[\n"
            fragment_files = FileList[File.join(Build::BuildDir, '**/*.json_frag')]
            # Write the first fragment separately
            File.open(fragment_files.first, "r") { |fragment_file|
                out_file.write fragment_file.read
            }
            # Write the rest with a comma in between
            fragment_files[1..-1].each do |fragment_filename|
                out_file.write(",\n")
                File.open(fragment_filename, "r") { |fragment_file|
                    out_file.write fragment_file.read
                }
            end

            out_file.write "\n]"
        }
    end
end