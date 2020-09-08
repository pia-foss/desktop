require_relative 'multifile.rb'

module BuildDSL
    include Rake::DSL

    # Define a file task with parallel prerequisites - combines the effects of
    # file() and multitask()
    def multifile(*args, &block)
      MultiFile.define_task(*args, &block)
    end
end
