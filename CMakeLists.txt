cmake_minimum_required(VERSION 2.8)
project(matrix)

add_subdirectory(src)
add_subdirectory(slogger)
add_subdirectory(keychain)
add_subdirectory(unit_tests)
add_subdirectory(examples/ToyScope)
add_subdirectory(examples/Helloworld)

# To install in a non-standard (e.g. not /usr/lib)
# specify the installation directory on the command line
# when configuring. The make install will then install into
# that directory. Note this is similar to the --prefix= of
# autotools, so the actual directory will be under lib, bin, 
# include etc.
#
# cmake .. -DCMAKE_INSTALL_PREFIX=../_install
