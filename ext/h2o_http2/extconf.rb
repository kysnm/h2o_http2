require "mkmf"

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']

$CFLAGS << " #{ENV["CFLAGS"]}"
$CFLAGS << " -g"
$CFLAGS << " -O3" unless $CFLAGS[/-O\d/]
$CFLAGS << " -Wall -Wno-comment"

CWD = File.expand_path(File.dirname(__FILE__))
OPENSSL_DIR = File.join(CWD, 'openssl')
OPENSSL_BUILD_DIR = File.join(CWD, 'openssl-build')
H2O_DIR = File.join(CWD, 'h2o')

$LDFLAGS << " -L#{OPENSSL_BUILD_DIR}/lib"
$CPPFLAGS << " -I#{OPENSSL_BUILD_DIR}/include"

Dir.mkdir(OPENSSL_BUILD_DIR) if !Dir.exists?(OPENSSL_BUILD_DIR)

Dir.chdir OPENSSL_DIR do
  case RUBY_PLATFORM
  when /darwin/i
    system "./Configure darwin64-x86_64-cc --prefix=#{OPENSSL_BUILD_DIR}"
  else
    system "./config --prefix=#{OPENSSL_BUILD_DIR}"
  end
  system 'make'
  system 'make install'
end

Dir.chdir H2O_DIR do
  Dir.mkdir("build") if !Dir.exists?("build")

  Dir.chdir("build") do
    system "cmake -DOPENSSL_ROOT_DIR=#{OPENSSL_BUILD_DIR} -DOPENSSL_LIBRARIES=#{OPENSSL_BUILD_DIR}/lib .."
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
