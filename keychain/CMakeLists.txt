cmake_minimum_required(VERSION 2.8)

set(THIRDPARTYDIR "/home/gbt1/lib_devel")
include_directories( "." "../src" "${THIRDPARTYDIR}/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++14")

set(SOURCE_FILES
cmdparam.cc
cmdparam.h
main.cc
)

add_executable(keychain ${SOURCE_FILES})
target_link_libraries (keychain LINK_PUBLIC matrix -L${THIRDPARTYDIR}/lib yaml-cpp zmq readline rt boost_regex cfitsio)
