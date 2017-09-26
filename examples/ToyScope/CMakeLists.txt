cmake_minimum_required(VERSION 2.8)

include_directories( "." "../../src" "${THIRDPARTYDIR}/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++14")

set(SOURCE_FILES
ExAccumulator.cc
ExAccumulator.h
ExFFT.cc
ExFFT.h
ExProcessor.cc
ExProcessor.h
ExSignalGenerator.cc
ExSignalGenerator.h
toy_scope.cc
)

add_executable(toy_scope ${SOURCE_FILES})
target_link_libraries (toy_scope LINK_PUBLIC matrix matrix_extra -L${THIRDPARTYDIR}/lib yaml-cpp zmq fftw3 rt boost_regex cfitsio)
