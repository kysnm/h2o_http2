require "bundler/gem_tasks"
require "rspec/core/rake_task"

RSpec::Core::RakeTask.new(:spec)

task :default => :spec

require "rake/extensiontask"

Rake::ExtensionTask.new("h2o_http2") do |ext|
  ext.lib_dir = "lib/h2o_http2"
end
