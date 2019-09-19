cmake_minimum_required(VERSION 2.8)

include_directories( "." "${THIRDPARTYDIR}/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wcomment")
set(INCLUDE_FILES
    matrix/Architect.h
    matrix/Component.h
    matrix/DataInterface.h
    matrix/DataSink.h
    matrix/DataSource.h
    matrix/FiniteStateMachine.h
    matrix/fixed_buffer.h
    matrix/GenericDataConsumer.h
    matrix/GnuradioDataSource.h
    matrix/Keymaster.h
    matrix/log_t.h
    matrix/make_path.h
    matrix/masterdoc.h
    matrix/matrix_util.h
    matrix/Mutex.h
    matrix/NANutils.h
    matrix/netUtils.h
    matrix/ResourceLock.h
    matrix/RTDataInterface.h
    matrix/Semaphore.h
    matrix/SharedObjectRegistry.h
    matrix/string_format.h
    matrix/TCondition.h
    matrix/TestDataGenerator.h
    matrix/Thread.h
    matrix/ThreadLock.h
    matrix/Time.h
    matrix/tsemfifo.h
    matrix/yaml_util.h
    matrix/zmq_util.h
    matrix/ZMQContext.h
    matrix/ZMQDataInterface.h)


set(SOURCE_FILES
    Architect.cc
    Component.cc
    DataInterface.cc
    DataSink.cc
    GenericDataConsumer.cc
    Keymaster.cc
    log_t.cc
    make_path.cc
    matrix_util.cc
    Mutex.cc
    NANutils.cc
    netUtils.cc
    RTDataInterface.cc
    Semaphore.cc
    SharedObjectRegistry.cc
    TestDataGenerator.cc
    Thread.cc
    Time.cc
    string_format.cc
    yaml_util.cc
    zmq_util.cc
    ZMQContext.cc
    ZMQDataInterface.cc
)

add_library(matrix ${SOURCE_FILES})

# To install the .h files, try this recipie
# install(TARGETS matrix DESTINATION lib)
install(FILES ${INCLUDE_FILES} DESTINATION include/matrix)
install(TARGETS matrix DESTINATION lib)

