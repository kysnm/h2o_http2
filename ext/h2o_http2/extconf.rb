require "mkmf"

CWD = File.expand_path(File.dirname(__FILE__))
H2O_DIR = File.join(CWD, 'h2o')
Dir.chdir H2O_DIR do
  Dir.mkdir("build") if !Dir.exists?("build")

  Dir.chdir("build") do
    system 'cmake ..'
    system 'make libh2o'
  end
end

$DEFLIBPATH.unshift("#{H2O_DIR}/build")
dir_config('h2o', "#{H2O_DIR}/include", "#{H2O_DIR}/build")

create_makefile("h2o_http2/h2o_http2")
