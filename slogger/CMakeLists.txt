cmake_minimum_required(VERSION 2.8)

include_directories( "." "../src" "${THIRDPARTYDIR}/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++14")

set(SOURCE_FILES
    slogger.cc
    FITSLogger.cc
    FITSLogger.h
)

add_executable(slogger ${SOURCE_FILES})
target_link_libraries (slogger LINK_PUBLIC matrix -L${THIRDPARTYDIR}/lib yaml-cpp zmq rt boost_regex cfitsio)
