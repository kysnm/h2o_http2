require "mkmf"

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']

$CFLAGS << " #{ENV["CFLAGS"]}"
$CFLAGS << " -g"
$CFLAGS << " -O3" unless $CFLAGS[/-O\d/]
$CFLAGS << " -Wall -Wno-comment"

$LDFLAGS << " -L`brew --prefix openssl`/lib"
$CPPFLAGS << " -I`brew --prefix openssl`/include"

CWD = File.expand_path(File.dirname(__FILE__))
H2O_DIR = File.join(CWD, 'h2o')

Dir.chdir H2O_DIR do
  Dir.mkdir("build") if !Dir.exists?("build")

  Dir.chdir("build") do
    system 'cmake -DOPENSSL_ROOT_DIR=`brew --prefix openssl` -DOPENSSL_LIBRARIES=`brew --prefix openssl`/lib ..'
    system 'make libh2o'
  end
end

LIBDIR     = RbConfig::CONFIG['libdir']
INCLUDEDIR = RbConfig::CONFIG['includedir']

HEADER_DIRS = [INCLUDEDIR, "#{H2O_DIR}/include"]

# setup constant that is equal to that of the file path that holds that static libraries that will need to be compiled against
LIB_DIRS = [LIBDIR, "#{H2O_DIR}/build"]

# array of all libraries that the C extension should be compiled against
libs = ['-lh2o', '-luv', '-lssl', '-lcrypto']

dir_config('h2o', HEADER_DIRS, LIB_DIRS)

# iterate though the libs array, and append them to the $LOCAL_LIBS array used for the makefile creation
libs.each do |lib|
  $LOCAL_LIBS << "#{lib} "
end

create_makefile("h2o_http2/h2o_http2")
