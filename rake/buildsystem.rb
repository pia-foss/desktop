require 'rake' # prerequisite, naturally
require_relative 'util/buildenv' # Before all others, this injects env vars from .buildenv
require_relative 'model/build'
require_relative 'executable'
require_relative 'install'
require_relative 'archive'
require_relative 'product/version'
require_relative 'product/translations'
require_relative 'product/breakpad'
require_relative 'product/unittest'
require_relative 'product/frameworks' if Build.xnuKernel?
require_relative 'product/desktop' if Build.desktop?
require_relative 'crowdin'
