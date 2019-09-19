cmake_minimum_required(VERSION 2.8)

include_directories( "." "../src" "${THIRDPARTYDIR}/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++11")

set(SOURCE_FILES
ArchitectTest.cc
ArchitectTest.h
keymaster_test.cc
keymaster_test.h
matrix_unittest.cc
ResourceLockTest.cc
ResourceLockTest.h
StateTransitionTest.cc
StateTransitionTest.h
TimeTest.cc
TimeTest.h
TransportTest.cc
TransportTest.h
TSemfifoTest.cc
TSemfifoTest.h
utility_test.cc
utility_test.h
)

add_executable(matrix_test ${SOURCE_FILES})
target_link_libraries (matrix_test LINK_PUBLIC matrix
 -L${THIRDPARTYDIR}/lib
 -L${THIRDPARTYDIR}/lib64
 cppunit yaml-cpp zmq rt boost_regex cfitsio)

