cmake_minimum_required(VERSION 2.8)

include_directories( "." "../src" "${THIRDPARTYDIR}/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++11")

set(SOURCE_FILES
cmdparam.cc
cmdparam.h
main.cc
)

add_executable(keychain ${SOURCE_FILES})
target_link_libraries (keychain LINK_PUBLIC matrix
 -L${THIRDPARTYDIR}/lib
 -L${THIRDPARTYDIR}/lib64
 yaml-cpp zmq readline rt boost_regex cfitsio)
