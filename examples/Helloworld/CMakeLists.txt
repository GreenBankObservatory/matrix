cmake_minimum_required(VERSION 2.8)

include_directories( "." "../../src" "${THIRDPARTYDIR}/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++14")

set(SOURCE_FILES
hello_world.cc
)

add_executable(hellow_matrix ${SOURCE_FILES})
target_link_libraries (hellow_matrix LINK_PUBLIC matrix -L${THIRDPARTYDIR}/lib yaml-cpp zmq fftw3 rt boost_regex cfitsio)
