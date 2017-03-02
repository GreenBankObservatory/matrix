cmake_minimum_required(VERSION 2.8)

include_directories( "." "../src" "/home/gbt1/lib_devel/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(INCLUDE_FILES
    FileDataSource.h
    FileDataSink.h)


set(SOURCE_FILES
    FileDataSource.cc
    FileDataSink.cc
)

add_library(matrix_extra ${SOURCE_FILES})

# To install the .h files, try this recipie
# install(TARGETS matrix DESTINATION lib)
install(FILES ${INCLUDE_FILES} DESTINATION include/matrix)
install(TARGETS matrix_extra DESTINATION lib)

